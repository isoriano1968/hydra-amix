

/*
 * A2091 driver (to the memory of Jeff Boyer)
 *
 * Certified for use with rev. D of the DMAC chip.  Avoid earlier revs. or
 * face certain data corruption.
 */

#include	"sys/types.h"
#include	"memory.h"
#include	"rico.h"
#include	"sd.h"


struct device {
	uchar		xxxxxxx0[0x40];
	ushort		istr,
			cntr;
	uchar		xxxxxxx1[0x3C];
	uint		wtc;
	ushort		*sac;
	uchar		xxxxxxx2[0x06];
	ushort		dawr;
	uchar		xxxxxxx3,
			scsi_a,
			xxxxxxx4,
			scsi_n,
			xxxxxxx5[0x4C];
	ushort		st_dma,
			sp_dma,
			clr_int;
	uchar		xxxxxxx6[0x02];
	ushort		flush;
};
/*
 * device.scsi_a (WD33C93A scsi registers)
 */
#define	AS	0x1F		/* auxiliary status */
#define	ID	0x00		/* own ID */
#define	CON	0x01		/* control */
#define	TP	0x02		/* timeout period */
#define	CDB	0x03		/* command descriptor block */
#define	TL	0x0F		/* target LUN, aka.. */
#define	TS	0x0F		/* ..target status */
#define	CP	0x10		/* command phase */
#define	TC	0x12		/* transfer count */
#define	DI	0x15		/* destination ID */
#define	SI	0x16		/* source ID */
#define	SS	0x17		/* SCSI status */
#define	COM	0x18		/* command */
#define	DR	0x19		/* data reg */
/*
 * device.scsi_n (WD33C93A scsi register bits)
 */
#define	CIP	(1 << 4)	/* AS - command in progress */
#define	DBR	(1 << 0)	/* AS - data buffer ready */
#define	EAF	(1 << 3)	/* ID - enable advanced features */
#define	DM2	(1 << 7)	/* CON - dma enabled */
#define	EDI	(1 << 3)	/* CON - ending disconnect interrupt */
#define	IDI	(1 << 2)	/* CON - intermediate disconnect interrupt */
#define	DPD	(1 << 6)	/* DI - data phase direction */
#define	ER	(1 << 7)	/* SI - enable reselection */

struct unit {
	volatile struct unit	*next;
	volatile struct sdcom	*comhead,
			*comtail;
	uchar		tc0,
			tc1,
			tc2;
};
struct card {
	volatile struct device	*device;
	volatile struct unit	units[SDUNITS],
			*curunitp,
			*starthead,
			*starttail;
	uint		istate,
			tc,
			startcom;
	ushort		*chipaddr;
	bool		initialized;
	char		xxxxxxxx[30];
};

/*
 * states of the interrupt DFA, as stored in `istate'
 */
#define	IDLE		0	/* 3393A is ready to start new request */
#define	STARTING	1	/* new request has been started */
#define	RESUMING	2	/* request has resumed */
#define	DEAD		3	/* driver has shut down due to bad input */
#define	MESSAGING	4	/* message is being read */
#define	ENDING		5	/* driver is awaiting request completion */

/*
 * Convert 3393A status code to input code for interrupt DFA.
 *	status	input	meaning
 *	------	-----	-------
 *	42	0	There is no device with the specified SCSI ID.
 *	-	1	Status is illegal or unsupported.
 *	21	2	A Save Data Pointers message has arrived.
 *	85	3	The target has temporarily disconnected.
 *	81	4	A target has reconnected.
 *	4F	5	A message, probably Restore Data Pointers, has arrived.
 *	20	6	The target is waiting for message acceptance.
 *	4B	7	Request status has arrived.
 *	16	8	Request has completed.
 */
static uchar itab[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	6, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 7, 1, 1, 1, 5,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 4, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
#define	NINPUT	9

/*
 * Given current DFA state and input, specify the action to be taken, which
 * will determine the new state.
 *	action	new state	meaning
 *	------	---------	-------
 *	0	IDLE,STARTING	Complete request, perhaps start new.
 *	1	DEAD		Report that driver has shut down.
 *	2	RESUMING,DEAD	Requeue request, perhaps resume other.
 *	3	RESUMING,DEAD	Perhaps resume request after reconnection.
 *	4	RESUMING	Resume request after 0x20.
 *	5	IDLE,STARTING	Abort request, perhaps start new.
 *	6	RESUMING	Save pointers, then resume.
 *	7	IDLE,STARTING	Disconnect request, perhaps start new.
 *	8	DEAD,MESSAGING	Perhaps read Restore Pointers.
 *	9	ENDING		Resume request, expecting completion.
 */
static uchar atab[][NINPUT] = {
	1,  1,  1,  1,  3,  1,  1,  1,  1,
	5,  1,  6,  7,  2,  8,  1,  9,  0,
	1,  1,  6,  7,  1,  8,  1,  9,  0,
	1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  4,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  0,
};

static volatile struct card	card[SDCARDS];


a2091queue( c, rp)
volatile struct sdcom	*rp;
{
	volatile struct card	*cp;
	volatile struct unit	*up;

	cp = &card[c];
	initialize( cp);
	up = &cp->units[rp->unit];
	rp->okay = FALSE;
	rp->next = 0;
	if (up->comhead) {
		up->comtail->next = rp;
		up->comtail = rp;
	}
	else {
		up->comhead = rp;
		up->comtail = rp;
		cipwait( cp->device);
		uqueue( cp, up);
	}
}


static
uqueue( cp, up)
volatile struct card	*cp;
volatile struct unit	*up;
{

	up->next = 0;
	if (cp->starthead) {
		cp->starttail->next = up;
		cp->starttail = up;
	}
	else {
		cp->starthead = up;
		cp->starttail = up;
		startany( cp);
	}
}


static
startany( cp)
volatile struct card	*cp;
{
	volatile struct unit	*up;
	volatile struct sdcom	*rp;
	volatile struct device	*dp;
	uint		i;

	unless ((cp->istate == IDLE)
	and (up = cp->starthead))
		return;
	cp->starthead = up->next;
	rp = up->comhead;
	cp->curunitp = up;
	dp = cp->device;
	for (i=0; i<nel( rp->cdb); ++i)
		setreg( dp, CDB+i, rp->cdb[i]);
	if (rp->reading)
		setreg( dp, DI, rp->unit|DPD);
	else
		setreg( dp, DI, rp->unit);
	setreg( dp, TL, 0);				/* future support */
	up->tc0 = byte2( rp->nbyte);
	up->tc1 = byte1( rp->nbyte);
	up->tc2 = byte0( rp->nbyte);
	startdma( cp, up);
	cp->istate = STARTING;
	setreg( dp, COM, cp->startcom);
}


a2091intr( )
{
	volatile struct card	*cp;

	for (cp=card; cp<endof( card); ++cp)
		if ((cp->device)
		and (cp->device->istr & 1<<4))
			service( cp);
}


static
service( cp)
volatile struct card	*cp;
{
	volatile struct unit	*up;
	volatile struct sdcom	*rp;
	volatile struct device	*dp;
	uint		ss;

	dp = cp->device;
	cipwait( dp);
	ss = reg( dp, SS);
	switch (atab[cp->istate][itab[ss]]) {
	case 0:
		stopdma( cp);
		up = cp->curunitp;
		rp = up->comhead;
		if (reg( dp, CP) == 0x60) {
			rp->okay = TRUE;
			rp->status = reg( dp, TS);
		}
		if (up->comhead = rp->next)
			uqueue( cp, up);
		(*rp->intr)( rp);
		cp->istate = IDLE;
		startany( cp);
		break;
	case 1:
		cp->istate = badhardware( ss, cp);
		break;
	case 2:
		stopdma( cp);
		uqueue( cp, cp->curunitp);
	case 3:
		up = &cp->units[reg( dp, SI)&SDUNITS-1];
		unless (rp = up->comhead) {
			cp->istate = badhardware( ss, cp);
			break;
		}
		setreg( dp, TL, 0);
		setreg( dp, CP, 0x44);
		startdma( cp, up);
		if (rp->reading)
			setreg( dp, DI, rp->unit|DPD);
		else
			setreg( dp, DI, rp->unit);
		cp->curunitp = up;
		cp->istate = RESUMING;
		setreg( dp, COM, cp->startcom);
		break;
	case 4:
		up = cp->curunitp;
		setreg( dp, CP, 0x45);
		setreg( dp, TC+0, up->tc0);
		setreg( dp, TC+1, up->tc1);
		setreg( dp, TC+2, up->tc2);
		cp->istate = RESUMING;
		setreg( dp, COM, cp->startcom);
		break;
	case 5:
		stopdma( cp);
		up = cp->curunitp;
		rp = up->comhead;
		if (up->comhead = rp->next)
			uqueue( cp, up);
		(*rp->intr)( rp);
		cp->istate = IDLE;
		startany( cp);
		break;
	case 6:
		up = cp->curunitp;
		up->tc0 = reg( dp, TC+0);
		up->tc1 = reg( dp, TC+1);
		up->tc2 = reg( dp, TC+2);
		cp->istate = RESUMING;
		setreg( dp, COM, cp->startcom);
		break;
	case 7:
		stopdma( cp);
		cp->istate = IDLE;
		startany( cp);
		break;
	case 8:
		setreg( dp, COM, 0xA0);
		until (reg( dp, AS) & DBR)
			;
		if (reg( dp, DR) == 0x03)
			cp->istate = MESSAGING;
		else
			cp->istate = badhardware( ss, cp);
		break;
	case 9:
		setreg( dp, TC+0, 0);
		setreg( dp, TC+1, 0);
		setreg( dp, TC+2, 0);
		setreg( dp, CP, 0x46);
		cp->istate = ENDING;
		setreg( dp, COM, cp->startcom);
	}
}


static
startdma( cp, up)
volatile struct card	*cp;
volatile struct unit	*up;
{
	volatile struct device	*dp;
	volatile struct sdcom	*rp;

	dp = cp->device;
	setreg( dp, TC+0, up->tc0);
	setreg( dp, TC+1, up->tc1);
	setreg( dp, TC+2, up->tc2);
	unless (cp->tc = up->tc0<<16 | up->tc1<<8 | up->tc2)
		return;
	rp = up->comhead;
	if (rp->reading)
		dp->cntr = 1<<5 | 1<<4;
	else
		dp->cntr = 1<<5 | 1<<4 | 1<<3;
	if (rp->addr >= (caddr_t)0x1000000) {
		unless (cp->chipaddr = AllocMem( cp->tc, MEMF_CHIP))
			panic( "a2091: no chip mem");
		unless (rp->reading)
			bcopy( rp->addr+rp->nbyte-cp->tc, cp->chipaddr, cp->tc);
		dp->sac = cp->chipaddr;
	}
	else
		dp->sac = rp->addr + rp->nbyte - cp->tc;
	dp->st_dma = TRUE;
}


static
stopdma( cp)
volatile struct card	*cp;
{
	volatile struct device	*dp;

	unless (cp->tc)
		return;
	dp = cp->device;
	dp->flush = TRUE;
	until (dp->istr & 1<<0)
		;
	dp->clr_int = TRUE;
	dp->sp_dma = TRUE;
	if (cp->chipaddr) {
		volatile struct sdcom *rp = cp->curunitp->comhead;
		if (rp->reading)
			bcopy( cp->chipaddr, rp->addr+rp->nbyte-cp->tc, cp->tc);
		FreeMem( cp->chipaddr, cp->tc);
		cp->chipaddr = 0;
	}
}


static
initialize( cp)
volatile struct card	*cp;
{
	volatile struct device	*dp,
			*a,
			*o;

	if (cp->initialized)
		return;
	cp->initialized = TRUE;
	if (not autocon( 0x02020003, cp-card, &a, &o))
		panic( "a2091 initialize");
	dp = a;
	cp->device = dp;
	reg( dp, SS);
	setreg( dp, ID, EAF|SDUNITS-1);
	setreg( dp, COM, 0x00);
	until (reg( dp, AS) & 1<<7)
		;
	cp->startcom = reg( dp, SS)? 0x08: 0x09;
	setreg( dp, CON, DM2|EDI|IDI);
	setreg( dp, SI, ER);
	setreg( dp, TP, 0xFF);
}


static
reg( dp, a)
volatile struct device	*dp;
{

	dp->scsi_a = a;
	return (dp->scsi_n);
}


static
setreg( dp, a, n)
volatile struct device	*dp;
{

	dp->scsi_a = a;
	dp->scsi_n = n;
}


static
cipwait( dp)
volatile struct device	*dp;
{

	while (reg( dp, AS) & CIP)
		;
}


static
badhardware( ss, cp)
volatile struct card	*cp;
{

	printf( "a2091: 0x%x %d 0x%x\n", ss, cp->istate, cp->curunitp);
	return (DEAD);
}
