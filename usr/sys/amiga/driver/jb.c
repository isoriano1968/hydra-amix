/*
 * The Jeff Boyer bus allocator.
 */
#include "amigahr.h"
#include "amigahd.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/inline.h"


/*
 * Externally visible functions
 */
int jbcall();
void jbpass();
void jbintr();
int jbrreg();
void jbsreg();
int jbscuz();
int jbwait();
int jbstop();

/*
 * Configurable parameters.
 */
#define	NCON	2			/* Maximum number of controllers */
#define	L2AL	9			/* Log of command block alignment */
#define	TIME	10000			/* Timeout count */
#define	sphi	spl2			/* Priority */


/*
 * Variables.
 */
static int	jbbusy[NCON];		/* Controller is busy */
static int	jbscsi[NCON];		/* Current mode */
static int	(*jbfunc[NCON])();	/* Start function */

volatile struct hdreg	*jbaddr[NCON];		/* Controller address */
volatile struct hdcom	*jbcomp[NCON];		/* Controller pointer */


/*
 * Storage for command blocks.
 */
static unsigned char jbcomt[NCON*(1<<L2AL) + sizeof(struct hdcom)-1];

/*
 * Private functions
 */
static int jbinit();
static void setmode();

/*
 * Allocate the Boyer bus.
 */
int
jbcall(bm, cn, fp)
int	bm;
int	cn;
int	(*fp)();
{
	if (cn >= NCON)
		return (0);
	if (jbaddr[cn] == 0) {
		if (jbinit(cn) == 0)
			return (0);
	}

	if (jbbusy[cn]!=0 && bm!=jbscsi[cn])
		jbfunc[cn] = fp;
	else {
		jbbusy[cn] = 1;
		setmode(cn, bm);
		(*fp)(cn, jbaddr[cn], jbcomp[cn]);
	}
	return (1);
}


/*
 * Initialise the controller.
 */
static int
jbinit(cn)
int cn;
{
	register volatile struct hdcom	*cp;
	register volatile struct hdreg	*rp;
	int			in;
	int			tn;
	long			ca;
	long			ba;
	long			bs;

	if (autocon(APNDISK, cn, &ba, &bs) == 0)
		return (0);
	rp = (volatile struct hdreg *)ba;
	jbaddr[cn] = rp;
	ca = (((((long)jbcomt)+((1<<L2AL)-1))
	      &~((1<<L2AL)-1))+(cn)*(1<<L2AL));

	rp->r_cont = 0;			/* reset the 2090 */
	delay(2);
	rp->r_cont = BCRES;
	delay(2);

	for (in=0; in<2; in++) {
		/* Send byte, pull CBP high, wait for high */
		tn = TIME;
		rp->r_data = ca >> (in?9:1);
		rp->r_cont = BCCBP|BCRES;
		while ((rp->r_cont&BCCBP) == 0) {
			if (--tn == 0)
				return (0);
		}

		/* Pull CBP low, wait for low response */
		tn = TIME;
		rp->r_cont = BCRES;
		while ((rp->r_cont&BCCBP) != 0) {
			if (--tn == 0)
				return (0);
		}
	}

	/* Enable interrupts */
	in = sphi();
	AMIGA->intena = AINTSET|AIEINT2;
	rp->r_cont = BCRES|BCSEL|BCIEN;

	jbrreg(rp, RSTAT);	/* Dismiss SCSI interrupt */
	jbsreg(rp, RIDEN, 0x07);/* Set SCSI ID */
	jbsreg(rp, RCOMM, 0x00);/* Execute reset */

	if (jbscuz(rp) == 0) {
		splx(in);
		return (0);
	}

	jbrreg(rp, RSTAT);	/* Dismiss SCSI interrupt */
	jbsreg(rp, RCONT, 0x80);/* Enable chip DMA */
	jbsreg(rp, RTIME, 0xFF);/* Set timeout period to FF */

	splx(in);
	jbscsi[cn] = 1;

	jbcomp[cn] = (volatile struct hdcom *)ca;

	delay(HZ);			/* Bogus: Maxtor drives don't work */
					/* for a while after reset. */
	return (1);
}


/*
 * Free the Boyer bus.
 */
void
jbpass(cn)
int cn;
{
	if (jbfunc[cn] == 0) {
		jbbusy[cn] = 0;
		return;
	}

	setmode(cn, !jbscsi[cn]);
	(*jbfunc[cn])(cn, jbaddr[cn], jbcomp[cn]);
	jbfunc[cn] = 0;
}


/*
 * Set the mode of the Boyer bus appropriately.
 */
static void
setmode(cn, bm)
int cn;
int bm;
{
	if (jbscsi[cn] != bm)
		jbaddr[cn]->r_cont = bm ? (BCRES|BCSEL|BCIEN) : (BCRES|BCIEN);
	jbscsi[cn] = bm;
}


/*
 * Accept an interrupt.
 */
void
jbintr()
{
	register volatile struct hdreg	*rp;
	int			cn;
	int			in;

	for (cn=0; cn<NCON; cn++) {
		if ((rp=jbaddr[cn])==0 || (rp->r_intr&BICOM)==0)
			continue;
		if (jbscsi[cn] == 0) {
			hdintr(cn, jbaddr[cn], jbcomp[cn]);
			return;
		} else {
			a2090intr(cn, jbaddr[cn]);
			return;
		}
	}
}


/*
 * Read a SCSI register.
 */
int
jbrreg(rp, ra)
register volatile struct hdreg	*rp;
int			ra;
{
	rp->r_scsa = ra;
	return (rp->r_scsd);
}


/*
 * Write to a SCSI register.
 */
void
jbsreg(rp, ra, rd)
register volatile struct hdreg	*rp;
int			ra;
int			rd;
{
	rp->r_scsa = ra;
	rp->r_scsd = rd;
}


/*
 * Wait for a SCSI command to complete.
 */
int
jbscuz(rp)
register volatile struct hdreg *rp;
{
	int	tn;

	tn = TIME;
	while ((jbrreg(rp, RAUXS)&0x80) == 0) {
		if (--tn == 0)
			return (0);
	}
	return (1);
}


/*
 * Wait for an ST506 command to complete.
 */
int
jbwait(rp)
register volatile struct hdreg *rp;
{
	int	tn;

	tn = TIME;
	while ((rp->r_intr&BICOM) == 0) {
		if (--tn == 0)
			return (0);
	}
	return (1);
}


/*
 * Wait for interrupt line to stop.
 */
int
jbstop(rp)
register volatile struct hdreg *rp;
{
	int	tn;

	tn = TIME;
	while ((rp->r_intr&BICOM) != 0) {
		if (--tn == 0)
			return (0);
	}
	return (1);
}
