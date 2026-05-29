

/*
 * driver for A3000 internal SCSI
 *
 * Supports target disconnect/reselect.
 */

#include	"sys/types.h"
#include	"rico.h"
#include	"sd.h"


static volatile struct hardware *device;


struct hardware {
	uchar	xxxxxxx0[0x02];
	ushort	dawr;			/* DACK width */
	uchar	xxxxxxx1[0x06];
	ushort	cntr;			/* control */
	uint	*sac;			/* DMA address */
	uchar	xxxxxxx2[0x02];
	ushort	sdma;			/* start DMA (strobe) */
	uchar	xxxxxxx3[0x02];
	ushort	fdma;			/* flush DMA (strobe) */
	uchar	xxxxxxx4[0x02];
	ushort	cint;			/* clear interrupts (strobe) */
	uchar	xxxxxxx5[0x02];
	ushort	istr;			/* interrupt status */
	uchar	xxxxxxx6[0x1E];
	ushort	srst;			/* stop DMA (strobe) */
	uchar	xxxxxxx7[0x01],
		scsi_a,
		xxxxxxx8[0x01],
		scsi_n;
};
/*
 * hardware.scsi_a (WD33C93A scsi registers)
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
 * hardware.scsi_n (WD33C93A scsi register bits)
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
static volatile struct unit
			units[SDUNITS],
			*curunitp,
			*starthead,
			*starttail;
static uint		istate,
			startcom	= 0x09;
static bool		dma_on;


a3091queue( con, cp)
volatile struct sdcom	*cp;
{
	volatile struct unit	*up;

	initialize( );
	up = &units[cp->unit];
	cp->okay = FALSE;
	cp->next = 0;
	if (up->comhead) {
		up->comtail->next = cp;
		up->comtail = cp;
	}
	else {
		up->comhead = cp;
		up->comtail = cp;
		cipwait( );
		uqueue( up);
	}
}


static
uqueue( up)
volatile struct unit	*up;
{

	up->next = 0;
	if (starthead) {
		starttail->next = up;
		starttail = up;
	}
	else {
		starthead = up;
		starttail = up;
		startany( );
	}
}


static
startany( )
{
	volatile struct unit	*up;
	volatile struct sdcom	*cp;
	uint		i;

	unless ((istate == IDLE)
	and (up = starthead))
		return;
	starthead = up->next;
	cp = up->comhead;
	curunitp = up;
	for (i=0; i<nel( cp->cdb); ++i)
		setreg( CDB+i, cp->cdb[i]);
	if (cp->reading)
		setreg( DI, cp->unit|DPD);
	else
		setreg( DI, cp->unit);
	setreg( TL, 0);				/* future support */
	up->tc0 = byte2( cp->nbyte);
	up->tc1 = byte1( cp->nbyte);
	up->tc2 = byte0( cp->nbyte);
	startdma( up);
	setreg( CON, DM2|EDI|IDI);
	istate = STARTING;
	setreg( COM, startcom);
}


a3091intr( )
{
	volatile struct unit	*up;
	volatile struct sdcom	*cp;
	uint		ss;

	unless ((device)
	and (device->istr & 1<<4))
		return;
	cipwait( );
	ss = reg( SS);
	switch (atab[istate][itab[ss]]) {
	case 0:
		stopdma( );
		up = curunitp;
		cp = up->comhead;
		if (reg( CP) == 0x60) {
			cp->okay = TRUE;
			cp->status = reg( TS);
		}
		if (up->comhead = cp->next)
			uqueue( up);
		(*cp->intr)( cp);
		istate = IDLE;
		startany( );
		break;
	case 1:
		istate = badhardware( ss);
		break;
	case 2:
		stopdma( );
		uqueue( curunitp);
	case 3:
		up = &units[reg( SI)&SDUNITS-1];
		unless (cp = up->comhead) {
			istate = badhardware( ss);
			break;
		}
		setreg( TL, 0);
		setreg( CP, 0x44);
		startdma( up);
		if (cp->reading)
			setreg( DI, cp->unit|DPD);
		else
			setreg( DI, cp->unit);
		setreg( CON, DM2|EDI|IDI);
		curunitp = up;
		istate = RESUMING;
		setreg( COM, startcom);
		break;
	case 4:
		setreg( CP, 0x45);
		setreg( TC+0, curunitp->tc0);
		setreg( TC+1, curunitp->tc1);
		setreg( TC+2, curunitp->tc2);
		istate = RESUMING;
		setreg( COM, startcom);
		break;
	case 5:
		stopdma( );
		up = curunitp;
		cp = up->comhead;
		if (up->comhead = cp->next)
			uqueue( up);
		(*cp->intr)( cp);
		istate = IDLE;
		startany( );
		break;
	case 6:
		curunitp->tc0 = reg( TC+0);
		curunitp->tc1 = reg( TC+1);
		curunitp->tc2 = reg( TC+2);
		setreg( CON, DM2|EDI|IDI);
		istate = RESUMING;
		setreg( COM, startcom);
		break;
	case 7:
		stopdma( );
		istate = IDLE;
		startany( );
		break;
	case 8:
		setreg( COM, 0xA0);
		until (reg( AS) & DBR)
			;
		if (reg( DR) == 0x03)
			istate = MESSAGING;
		else
			istate = badhardware( ss);
		break;
	case 9:
		setreg( TC+0, 0);
		setreg( TC+1, 0);
		setreg( TC+2, 0);
		setreg( CP, 0x46);
		setreg( CON, DM2|EDI|IDI);
		istate = ENDING;
		setreg( COM, startcom);
	}
}


static
startdma( up)
volatile struct unit	*up;
{
	volatile struct sdcom	*cp;
	uint		n;

	setreg( TC+0, up->tc0);
	setreg( TC+1, up->tc1);
	setreg( TC+2, up->tc2);
	unless (n = up->tc0<<16 | up->tc1<<8 | up->tc2)
		return;
	cp = up->comhead;
	if (cp->reading)
		device->cntr = 1<<3 | 1<<2;
	else
		device->cntr = 1<<3 | 1<<2 | 1<<1;
	device->sac = cp->addr + cp->nbyte - n;
	device->sdma = TRUE;
	dma_on = TRUE;
}


static
stopdma( )
{

	unless (dma_on)
		return;
	device->fdma = TRUE;
	until (device->istr & 1<<0)
		;
	device->cint = TRUE;
	device->srst = TRUE;
	dma_on = FALSE;
}


static
initialize( )
{
	int		o;
	static bool	once;

	if (once)
		return;
	once = TRUE;
	unless ((autocon( 0x0202F003, 0, &device, &o))
	and (device == (volatile struct hardware *)0xDD0000))
		panic( "a3091");
	device->dawr = 0x03;
	reg( SS);
	setreg( ID, EAF|SDUNITS-1);
	setreg( COM, 0x00);
	until (reg( AS) & 1<<7)
		;
	if (reg( SS))
		startcom = 0x08;
	setreg( SI, ER);
	setreg( TP, 0xFF);
}


static
reg( a)
{

	device->scsi_a = a;
	return (device->scsi_n);
}


static
setreg( a, n)
{

	device->scsi_a = a;
	device->scsi_n = n;
}


static
cipwait( )
{

	while (reg( AS) & CIP)
		;
}


static
badhardware( ss)
{

	printf( "a3091: 0x%x %d 0x%x\n", ss, istate, curunitp);
	return (DEAD);
}
