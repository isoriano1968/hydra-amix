/*
 * A driver for the virtual billboard that allows a user process to
 * capture console output.
 */
#include "sys/types.h"
#include "sys/file.h"
#include "sys/param.h"
#include "sys/poll.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/inline.h"
#include "sys/sysmacros.h"
#include "sys/immu.h"

extern sptalloc();
extern void sptfree();


/*
 * Routines to interface to the real console.
 */
extern void (*conputc)();
extern char *panicstr;			/* If nonzero, "normal" putc is used */

#define	PSIZE	2048			/* Size of pipe buffer */


/*
 * Useful macro.
 */
#define	inc(i)	(i=((i+1)%PSIZE))


/*
 * Variables.
 */
static unsigned int	refc;		/* Reference count */
static unsigned int	rind;		/* Read index */
static unsigned int	wind;		/* Write index */
static unsigned char	*buff;		/* Buffer (zero iff device closed) */
static void (*save_putc)();		/* Saved conputc value */

static struct pollhead bbpollist;	/* list of all pollers to wake up */

/* bbputc() replaces putchar() when the bb device is open. */
static void bbputc(c)
unsigned char c;
{
	register int	i;

	if (panicstr) {
		(*save_putc)(c);
		return;
	}

	if (rind == wind)
		wakeup(&rind);
	if (bbpollist.ph_events & POLLRDNORM)
	    pollwakeup(&bbpollist, POLLRDNORM);
	if (bbpollist.ph_events & POLLIN)
	    pollwakeup(&bbpollist, POLLIN);

	buff[wind] = c;
	if (inc(wind) == rind) {
		i = inc(wind);
		printf(" ...\n");
		wind = rind;
		rind = i;
	}
}


/*
 * Open the virtual billboard.
 */
int bbopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
	if (getminor(*devp) != 0)
		return ENXIO;

	if (mode&FWRITE)
		return EIO;

	if (!buff) {
		save_putc = conputc;
		buff = (unsigned char *)kmem_alloc(PSIZE, 0);
		if (!buff)
			return ENOMEM;
		rind=wind=0;
		bbpollist.ph_events = 0;
		bbpollist.ph_list = NULL;
		conputc = bbputc;
	}

	return 0;
}


/*
 * Close the virtual billboard.
 */
int bbclose(dev, mode, cr)
dev_t dev;
int mode;
struct cred *cr;
{
	conputc = save_putc;
	kmem_free(buff, PSIZE);
	buff = (unsigned char *)0;

	return 0;
}


int bbread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	int s;

	s = splhi();
	while (rind == wind) {
		if (sleep(&rind, PCATCH|PZERO+1) != 0) {
			splx(s);
			return EINTR;
		}
	}
	splx(s);

	while (rind != wind && uiop->uio_resid) {
		int error;
		int n = (wind>rind) ? (wind-rind) : (PSIZE-rind);
		if (error=uiomove(buff+rind, n, UIO_READ, uiop))
			return error;
		if ( (rind+=n) >= PSIZE )
			rind=0;
	}

	return 0;
}

int bbpoll(dev, events, anyyet, reventsp, phpp)
dev_t dev;
short events;
int anyyet;
short *reventsp;
struct pollhead **phpp;
{
    register short retevents = 0;
    register int s = splhi();

    if (events & POLLWRNORM)
	retevents |= POLLOUT;
    if (wind != rind)
    {
	if (events & POLLRDNORM)
	    retevents |= POLLRDNORM;
	if (events & POLLIN)
	    retevents |= POLLIN;
    }

    *reventsp = retevents;
    if (retevents)
    {
	splx(s);
	return 0;
    }

    /*
     * If poll() has not found any events yet, set up event cell
     * to wake up the poll if a requested event occurs.
     */
    if (!anyyet)
	*phpp = &bbpollist;

    splx(s);
    return 0;
}
