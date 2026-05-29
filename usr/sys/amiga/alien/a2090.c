

#include	"sys/types.h"
#include	"amigahr.h"
#include	"rico.h"
#include	"sd.h"


struct info {
	volatile struct sdcom	*head,
			*tail;
	bool		busy,
			initialized;
};

struct device {
	uchar	xxx0[0x60],
		a,
		xxx1,
		r,
		xxx2,
		dma0,
		xxx3[0x03],
		dma1;
};
/* device.a (scsi registers)
 */
#define	AS	0x1F		/* auxiliary status */
#define	ID	0x00		/* own id */
#define	CON	0x01		/* control */
#define	TP	0x02		/* timeout period */
#define	CDB	0x03		/* command descriptor block */
#define	TL	0x0F		/* target LUN, aka.. */
#define	TS	0x0F		/* ..target status */
#define	CP	0x10		/* command phase */
#define	TC	0x12		/* transfer count */
#define	DI	0x15		/* destination ID */
#define	SS	0x17		/* SCSI status */
#define	COM	0x18		/* command */
#define	DR	0x19		/* data reg */


static volatile struct info	info[SDCARDS];
void			start( );
bool			stopdma( );


a2090queue( con, cp)
volatile struct sdcom	*cp;
{
	volatile struct info	*ip;

	ip = &info[con];
	cp->okay = FALSE;
	if (ip->head)
		ip->tail->next = cp;
	else
		ip->head = cp;
	ip->tail = cp;
	cp->next = 0;
	jbcall( TRUE, con, start);
}


static void
start( con, dp)
volatile struct device	*dp;
{
	volatile struct info	*ip;
	volatile struct sdcom	*cp;
	uint		i;

	ip = &info[con];
	if (ip->busy)
		return;
	cp = ip->head;
	if (not cp) {
		jbpass( con);
		return;
	}
	initialize( ip, dp);
	startdma( dp, cp->reading, cp->addr);
	for (i=0; i<nel( cp->cdb); ++i)
		setreg( dp, CDB+i, cp->cdb[i]);
	setreg( dp, TC+0, byte2( cp->nbyte));
	setreg( dp, TC+1, byte1( cp->nbyte));
	setreg( dp, TC+2, byte0( cp->nbyte));
	setreg( dp, DI, cp->unit);
	/*
	 * What about Target LUN?
	 */
	setreg( dp, COM, 0x09);
	ip->busy = TRUE;
}


a2090intr( con, dp)
volatile struct device	*dp;
{
	volatile struct info	*ip;
	volatile struct sdcom	*cp;
	uint		ss;

	ip = &info[con];
	initialize( ip, dp);
	ss = reg( dp, SS);
	cp = ip->head;
	if (not cp)
		return;
	switch (ss) {
	case 0x42:
		stopdma( dp);
		ip->head = cp->next;
		(*cp->intr)( cp);
		ip->busy = FALSE;
		start( con, dp);
		break;
	case 0x4B:
		setreg( dp, COM, 0xA0);
		while (not (reg( dp, AS)&0x01))
			;
		cp->okay = TRUE;
		cp->status = reg( dp, DR);
		break;
	case 0x1F:
		setreg( dp, COM, 0xA0);
		while (not (reg( dp, AS)&0x01))
			;
		reg( dp, DR);
		break;
	case 0x20:
		setreg( dp, COM, 0x03);
		break;
	case 0x85:
		if (not stopdma( dp))
			printf( "sd: dma retry\n");
		else {
			if (reg( dp, CP) == 0x60) {
				cp->okay = TRUE;
				cp->status = reg( dp, TS);
			}
			ip->head = cp->next;
			(*cp->intr)( cp);
		}
		ip->busy = FALSE;
		start( con, dp);
	case 0x16:
	case 0x00:
		break;
	default:
		printf( "sd: unknown ss=0x%x\n", ss);
	}
}


static
reg( dp, a)
volatile struct device	*dp;
{

	dp->a = a;
	return (dp->r);
}


static
setreg( dp, a, r)
volatile struct device	*dp;
{

	dp->a = a;
	dp->r = r;
}


static
startdma( dp, reading, a)
volatile struct device	*dp;
bool		reading;
{

	a /= sizeof( short);
	if (not reading)
		a |= 0x800000;
	setdma( dp, 0xFB, byte2( a));
	setdma( dp, 0xFD, byte1( a));
	setdma( dp, 0xF6, byte0( a));
	dp->dma0 = 0xF7;
}


static bool
stopdma( dp)
volatile struct device	*dp;
{
	uint	i;
	uchar	s;

	dp->dma0 = 0xEF;
	for (i=0; i<10; ++i)
		;
	s = dp->dma1;
	dp->dma0 = 0x7F;
	for (i=0; i<10; ++i)
		;
	dp->dma0 = 0xFF;
	for (i=0; i<10; ++i)
		;
	return (s & 1<<5);
}


static
setdma( dp, c, d)
volatile struct device	*dp;
{
	uint	i;

	dp->dma0 = c;
	for (i=0; i<10; ++i)
		;
	dp->dma1 = d;
	for (i=0; i<10; ++i)
		;
}


static
initialize( ip, dp)
volatile struct info	*ip;
volatile struct device	*dp;
{

	if (ip->initialized)
		return;
	ip->initialized = TRUE;
	AMIGA->intena = AINTSET | AIEINT2;
	reg( dp, SS);
	setreg( dp, ID, SDUNITS-1);
	setreg( dp, COM, 0);
	while (not (reg( dp, AS)&0x80))
		;
	reg( dp, SS);
	setreg( dp, CON, 0x80);
	setreg( dp, TP, 0xFF);
}
