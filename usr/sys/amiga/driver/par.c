/*
 * par.c : Generic Amiga parallel port driver.
 * This version for System V Release 4.0.
 *
 * This is NOT a printer driver.  It is a generic driver for the
 * Amiga parallel port.  It currently supports output only, but it
 * should eventually support input as well.  It is a "raw" interface
 * to the port, and therefore doesn't do anything stupid like count
 * newlines or insert form-feeds.  For an "lp" style interface,
 * some filtering might have to be done on a higher level.
 *
 * Still to do:
 *	Add input capability.
 *
 *	Have sleeping writer awakened when outq gets near empty.
 */


#include "sys/types.h"
#include "sys/conf.h"
#include "sys/param.h"
#include "sys/poll.h"
#include "sys/uio.h"
#include "sys/file.h"
#include "sys/errno.h"
#include "sys/tty.h"
#include "sys/sysmacros.h"
#include "sys/inline.h"		/* Needed for spl*() */
#include "amigahr.h"
#include "par.h"

#ifdef DEBUG
#define dprintf(n) printf n
#else
#define dprintf(n)
#endif /* DEBUG */


/* This is the cpu priority level for all parallel-related processing */
#define splpar()	spl2()


static int parflags;			/* Various flags */
#define	P_INPUT		0x01		/* Open for input mode */
#define	P_OUTPUT	0x02		/* Open for output mode */
#define	P_OPEN		0x03		/* NZ if the device is open */
#define P_INPSLP	0x04		/* Sleeping because inpq is full */
#define P_OUTSLP	0x08		/* Sleeping because outq is full */
#define P_FLUSHING	0x10		/* Sleeping until queue is empty */
#define P_OUTPEND	0x20		/* Output character is pending */
#define P_OUTSTOP	0x40		/* Stopped due to control lines */
#define P_OUTERR	0x80		/* An output character timed out */


/* Hardware parallel control bits in ACIAB->pra */
#define PSEL		0x04		/* Select */
#define PPOUT		0x02		/* Paper Out */
#define PBUSY		0x01		/* Busy */

static short control_bits = PSEL|PPOUT|PBUSY;


static struct pollhead parpollist;	/* list of all pollers to wake up */

static struct clist par_inpq;			/* Input queue */
static struct clist par_outq;			/* Output queue */
#define PAR_LOWAT	100
#define PAR_HIWAT	400


/* sleep() priorities */
#define PARIPRI		TTIPRI
#define PAROPRI		TTOPRI


/* If Output is not acked within PARTIMEOUT ticks, an I/O Error is flagged */
#define PARTIMEOUT	(HZ*60)		/* Sixty seconds */
static int timeout_id;

static unsigned char currentbyte;
static void parstart(), partimeout();


/* This init function just makes everything */
/* be quiet until the device is opened. */
void parinit()
{
	ACIAB->ddra &= ~(PSEL|PPOUT|PBUSY); /* Set control lines to input */
	ACIAA->ddrb = 0;		/* Set all 8 data bits to input */
	ACIAA->icr = ICR_CLR|ICR_FLG;	/* Disable "Flag" Interrupt */
}


int paropen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
	int s;

	if (getminor(*devp))
		return ENODEV;

	/* Only output is supported for now */
	if (mode&FREAD)
		return EIO;

	s = splpar();

	ACIAB->ddra &= ~(PSEL|PPOUT|PBUSY); /* Set control lines to input */
	ACIAA->ddrb = 0xff;		/* Set all 8 data bits to output */
	ACIAA->icr = ICR_SET|ICR_FLG;	/* Enable "Flag" Interrupt */
	AMIGA->intena = AINTSET | AIEINT2; /* Just in case it wasn't already */
	parflags = P_OUTPUT;		/* Indicate that device is open */

	splx(s);

	return 0;
}


int parclose(dev, mode, type, cr)
dev_t dev;
int mode;
int type;
struct cred *cr;
{
	int s = splpar();

	/* wait for output to drain */
	while (parflags & P_OUTPEND) {
		parflags |= P_FLUSHING;
		if (sleep(&par_outq, PAROPRI|PCATCH)) {
			untimeout(timeout_id);
			parflags &= ~P_OUTPEND;
			while (getc(&par_outq) != -1)
				;
		}
	}
	parflags = 0;
	splx(s);

	ACIAB->ddra &= ~(PSEL|PPOUT|PBUSY); /* Set control lines to input */
	ACIAA->ddrb = 0;		/* Set all 8 data bits to input */
	ACIAA->icr = ICR_CLR|ICR_FLG;	/* Disable "Flag" Interrupt */

	return 0;
}


int parread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	/* Not implemented */
	return EINVAL;
}


int parwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	int s;
	register int c;

	if (!(parflags & P_OUTPUT))
		return EINVAL;

	s = splpar();

	while ( (c=uwritec(uiop)) != -1 ) {
		while (!(parflags & P_OUTERR) &&
		       par_outq.c_cc > PAR_HIWAT) {
			parflags |= P_OUTSLP;
			sleep(&par_outq, PAROPRI);
		}
		if (parflags & P_OUTERR) {
			parflags &= ~P_OUTERR;
			splx(s);
			return EIO;
		}
		if (!(parflags & P_OUTPEND))
			parstart(c);
		else
			putc(c, &par_outq);
	}

	if (uiop->uio_fmode & FSYNC) {
		/* wait for output to drain (or error to occur) */
		while ((parflags & P_OUTPEND) && !(parflags & P_OUTERR)) {
			parflags |= P_FLUSHING;
			sleep(&par_outq, PAROPRI);
		}
		if (parflags & P_OUTERR) {
			parflags &= ~P_OUTERR;
			splx(s);
			return EIO;
		}
	}

	splx(s);
	return 0;
}


int parioctl(dev, cmd, arg, mode, cr, rvalp)
dev_t dev;
int cmd;
int arg;
int mode;
struct cred *cr;
int *rvalp;
{
	switch (cmd) {
	case PIOCGETCTL:
		*rvalp = control_bits;
		break;
	case PIOCSETCTL:
		control_bits = arg & (C_SEL|C_POUT|C_BUSY);
		break;
	default:
		/* Unknown ioctl */
		return EINVAL;
	}

	return 0;
}


/*
 * Determines whether the necessary conditions exist for
 * the port to be readable, writeable, or have exceptions.
 */
int parpoll(dev, events, anyyet, reventsp, phpp)
dev_t dev;
short events;
int anyyet;
short *reventsp;
struct pollhead **phpp;
{
	register short retevents = 0;
	register int s = splpar();

	/* Read not implemented; always ready */
	retevents |= (events&(POLLIN|POLLRDNORM));
	if ((parflags & P_OUTERR) ||
	    par_outq.c_cc <= PAR_HIWAT)
		retevents |= (events & (POLLWRNORM|POLLOUT));

	*reventsp = retevents;
	if (retevents)
	{
		splx(s);
		return 0;
	}

	/*
	 * If poll() has not found any events yet, set up event cell
	 * to wake up the poll if a requested event occurs on this
	 * stream.  Check for collisions with outstanding poll requests.
	 */
	if (!anyyet)
		*phpp = &parpollist;

	splx(s);
	return 0;
}


/*
 * Parallel port interrupt routine, called from trap.s (via aciaaintr)
 * during level 2 interrupt when ACIAA->icr says that the parallel port
 * (aciaa "F" input) actually is interrupting.  See trap.s, acia.c.
 */
void parintr()
{
	if (parflags & P_OUTPEND) {
		untimeout(timeout_id);
		if (par_outq.c_cc)
			parstart(getc(&par_outq));
		else {
			parflags &= ~P_OUTPEND;
			if (parflags & (P_FLUSHING|P_OUTSLP)) {
				parflags &= ~(P_FLUSHING|P_OUTSLP);
				wakeup(&par_outq);
			}
			if (parpollist.ph_events & POLLWRNORM) 
				pollwakeup(&parpollist, POLLWRNORM);
			if (parpollist.ph_events & POLLOUT) 
				pollwakeup(&parpollist, POLLOUT);
		}
	}
	else if (parflags & P_INPUT) {
		/* Not implemented */
	}
	else
		dprintf(("Spurious parallel interrupt!\n"));
}


static void partimeout()
{
	if (!((ACIAB->pra ^ C_SEL) & control_bits)) {
		dprintf(("partimeout:  [timeout error]\n"));
		parflags |= P_OUTERR;
		if (parflags & (P_FLUSHING|P_OUTSLP)) {
			parflags &= ~(P_FLUSHING|P_OUTSLP);
			wakeup(&par_outq);
		}
		if (parpollist.ph_events & POLLWRNORM) 
			pollwakeup(&parpollist, POLLWRNORM);
		if (parpollist.ph_events & POLLOUT) 
			pollwakeup(&parpollist, POLLOUT);
	}
	parstart(currentbyte);
}


static void parstart(ch)
unsigned char ch;
{
	currentbyte = ch;
	parflags |= P_OUTPEND;

	if ((ACIAB->pra ^ C_SEL) & control_bits) {
		parflags |= P_OUTSTOP;
		dprintf(("parstart:  sleeping for a sec (because 0x%x)...\n",
			 (ACIAB->pra ^ C_SEL) & control_bits));
		timeout_id = timeout(parstart, currentbyte, HZ);
	}
	else {
		parflags &= ~P_OUTSTOP;
		ACIAA->prb = ch;
		timeout_id = timeout(partimeout, 0, PARTIMEOUT);
	}
}
