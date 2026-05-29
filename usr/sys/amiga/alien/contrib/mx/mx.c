/*
   1/16/92  Jack Koehler, Palomax, Inc. 215-672-6815
   last modification: 3/22/92
   Patchlevel: mx.c_1.1
   Supports 4 serial ports of 2 - IBM clone 2s1p1g 8250/16450/16550 IO boards
   OR a 4 or 8 port "serial PC COM 8" board on a modified MAX-125 Inteface Adapter.
   Other boards should also operate.
   Uses rx external interrupt 6, tx external interrupt 2.
   Baud rates from 110 to 38400 & 57600 (75) & 115200 (50)

   The MAX-125 modifications are as follows:
	1. jumper from Amiga I/O pin 20 to IBM pin B7.

	2. Diode mods:
           NOTE: This mod not required if only IRQ5 will be used.

           Amiga I/O
	      21------->|-------|	1N914/1N4148 diodes
	      22------->|-------|
	      24------->|-------|
	      25------->|-------|__________ to junction of D1-D2-R3
                 anode    cathode

	   NOTE: Some IBM boards do not actively drive the interrupt line high.
		 For these boards, add a 470 to 510 ohm resistor to the IBM board
		 interrupt buffer output pin to +5vdc to provide the requiured drive.

        3. S5 is 7mhz to IBM board pin B20, S9 is 14mhz to IBM board pin B30.
	   S5 & S9 must be closed for boards having connections to these contacts.

	4. Modify the "portaddr" structure below for your boards address.

	5. Select the proper "ON" definition for out2, out1 states that enable
	   the chips interrupts.  You may have to try several to see which one is
	   proper.

To do:
	1. Add 16550 FIFO support
	2. Change TX interrupt to use level 1 instead of level 2
	3. Better sharing of tx interrupt service
*/



#include "sys/types.h"
#include "sys/param.h"
#include "sys/dir.h"
#include "sys/file.h"
#include "sys/signal.h"
#include "sys/termio.h"
#include "sys/termios.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/strtty.h"
#include "sys/errno.h"
#include "sys/conf.h"
#include "sys/sysmacros.h"
#include "sys/inline.h"
#include "sys/systm.h"
#include "sys/cred.h"
#include "sys/cmn_err.h"
#include "amigahr.h"


/*
   Comment/uncomment out or modify the following IBM board chip base address structs
   to match your board requirements AND select the proper setup for the variable "on"
   below.
*/

/* This address set is for up to 2 - 2s1p1g type boards */
/* requires on = ON3;					*/
static unsigned long portaddr[] =	/* offsets from boardbase */
{					/* times 2 for -MAX- access */
	0x3f8L*2L,			/* mx0 (com1) */
	0x2f8L*2L,			/* mx1 (com2) */
	0x3e8L*2L,			/* mx2 (com3) */
	0x2e8L*2L,			/* mx3 (com4) */
};

/* This address set is for a PC COM 4, four port board type boards */
/* requires on = ON0;					*/
/*static unsigned long portaddr[] =	/* offsets from boardbase */
/*{					/* times 2 for -MAX- access */
/*	0x2a0L*2L,			/* mx0 (com1) */
/*	0x2a8L*2L,			/* mx1 (com2) */
/*	0x2b0L*2L,			/* mx2 (com3) */
/*	0x2b8L*2L,			/* mx3 (com4) */
/*};

/* This address set is for a PC COM 8, eight port board type boards */
/* requires on = any;					*/
/*static unsigned long portaddr[] =	/* offsets from boardbase */
/*{					/* times 2 for -MAX- access */
/*	0x280L*2L,			/* mx0 (com1) */
/*	0x288L*2L,			/* mx1 (com2) */
/*	0x290L*2L,			/* mx2 (com3) */
/*	0x298L*2L,			/* mx3 (com4) */
/*	0x2a0L*2L,			/* mx4 (com5) */
/*	0x2a8L*2L,			/* mx5 (com6) */
/*	0x2b0L*2L,			/* mx6 (com7) */
/*	0x2b8L*2L,			/* mx7 (com8) */
/*};


/*
   This port (sub your addr or 0L if not required) may be required to enable the
   boards master interrupt from the pattern contained in intenmask
*/
/* static unsigned long intvector = 0x2c2*2L;	/* master int enable port */
static unsigned long intvector = 0L;		/* master int enable port (none) */
static unsigned char intenmask = 0xff;		/* master interrupt enable mask */


#define BUFsize   4096U				/* rx buffer size per port */
#define HIGHwater 256
#define LOWwater  128


typedef enum
{
    OPEN,
    CLOSE,
    SETBREAK
} xmitter;

typedef struct
{
    unsigned long	portbase;		/* base address of this chip */
    unsigned int	pss;
    int			active;			/* flag: 1 if this chip port active */
    int			minornum;		/* minor device number */
    int			inputglobsize;
    int 		carrier;		/* DCD state */
    int			cts;			/* cts state */
    int			output_xoff;		/* xoff state */
    struct queue	*rdq;			/* read queue pointer */
    mblk_t		*mp;			/* write message pointer */
    struct strtty	tty;
    mblk_t		*bp;
    mblk_t		*bp1;
    mblk_t		*bp2;
    struct termio	*cb;
    struct termios	*cbs;
    struct iocblk	*iocbp;
    struct iocblk	*iocbp1;
    struct strtty	*tp;
    unsigned int	head, tail;		/* rx fifo pointers */
    unsigned char	buff[BUFsize];		/* rx fifo */
    unsigned char	lcrc;
    unsigned char	interrupt;		/* chip IIR save */
    unsigned char	lsr;			/* chip LSR save */
    unsigned char	special;
    unsigned char	ch;			/* tx character */
    unsigned char	pad;
} mx_t;

#define blklen(bp)	(bp->b_wptr - bp->b_rptr)

#define input_mblk	tty.t_in.bu_bp
#define output_mblk	tty.t_out.bu_bp

#define hardware_flow_control	1

#define INPUTGLOBSIZE	(128)

#define FALSE (0)
#define TRUE (1)

static int nports = sizeof(portaddr)/4;	/* nummber of ports */


/* --- Various I/O definitions specific to IBM asynch I/O on the -MAX-125 --- */

#define MAX125 0x07ee0000		/* MAX-125 = Mfg 2030, Prod 0 */
#define MAXNO 0				/* first board found */

static unsigned long maxbase = 0;	/* base address ptr of first -MAX-125 board */


/* ------------ 8250/16450/16550 definitions ------------ */
/*							  */
/* Chip register offsets from chip base address		  */
/* maxbase = MAX-125 base address (from autocon() call)   */
/* portbase = maxbase + portaddr[n]			  */
/*							  */
/* *((unsigned char *)(mx->portbase+(unsigned long)reg)) = access */

/* 8250 register offset times 2 for MAX-125 access */
#define	TXR		0	/* Transmitter reg (write, DLAB=0) */
#define	RXR		0	/* Receiver reg     (read, DLAB=0) */
#define	DIVLSB		0	/* Divisor latch LSB      (DLAB=1) */
#define	DIVMSB		1*2	/* Divisor latch MSB      (DLAB=1) */
#define	INTEN		1*2	/* Interrupt enable reg   (DLAB=0) */
#define	IIR		2*2	/* Interrupt identification register (r/o) */
#define	FCR		2*2	/* FIFO control register (16550A only) (w/o) */
#define	LCR		3*2	/* Line control register */
#define	MCR		4*2	/* Modem control register */
#define	LSR		5*2	/* Line status register */
#define	MSR		6*2	/* Modem status register */
#define	SCRATCH		7*2	/* Modem scratch register */


/* 8250 DIVLSB/MSB - DIVISOR baud clock divisor */
/* 1.843200.0 Mhz / 16  (measured = 1.842920 Mhz) */
static unsigned short speeds[] =
{
	0,			/* 0 - hangup  */
	1,			/* 1 - 115200 only way to get this (replaces 50) */
	2,			/* 1 - 57600  only way to get this (replaces 75) */
	1047,			/* 3 -  110   */
	857,			/* 4 -  134.5 */
	768,			/* 5 -  150   */
	576,			/* 6 -  200   */
	384,			/* 7 -  300   */
	192,			/* 8 -  600   */
	96,			/* 9 - 1200   */
	64,			/* A - 1800   */
	48,			/* B - 2400   */ 
	24,			/* C - 4800   */
	12,			/* D - 9600   */
	6,			/* E - 19200   */
	3,			/* F - 38400   */
};


/* 8250 INTEN - Interrupt Enable Register bits */
#define	IENRBF		0x01	/* RX Data available interrupt enable */
#define	IENTBE		0x02	/* Tx buffer empty interrupt enable */
#define	IENRST		0x04	/* Receive line status interrupt enable */
#define	IENMST		0x08	/* Modem status interrupt enable */


/* 8250 IIR - Interrupt Identification Register bits */
#define	IIR_IP		0x01	/* 0 if interrupt pending */
#define	IIR_MSTAT	0x00	/* Modem status interrupt */
#define	IIR_THRE	0x02	/* Transmitter holding register empty int */
#define	IIR_RDA		0x04	/* Receiver data available interrupt */
#define	IIR_RLS		0x06	/* Receiver Line Status interrupt */
#define IIR_ID_MASK	0x06	/* Mask for interrupt ID */
#define IIR_FIFO_TIMEOUT 0x08	/* FIFO timeout interrupt pending - 16550A */
#define IIR_FIFO_ENABLED 0xc0	/* FIFO enabled (FCR0,1 = 1) - 16550A only */


/* 16550A FCR - FIFO control register values */
#define	FIFO_ENABLE	0x01	/* enable TX & RX fifo */
#define	FIFO_CLR_RX	0x02	/* clear RX fifo */
#define	FIFO_CLR_TX	0x04	/* clear TX fifo */
#define	FIFO_START_DMA	0x08	/* enable TXRDY/RXRDY pin DMA handshake */
#define FIFO_SIZE_1	0x00	/* RX fifo trigger levels */
#define FIFO_SIZE_4	0x40
#define FIFO_SIZE_8	0x80
#define FIFO_SIZE_14	0xC0
#define FIFO_SIZE_MASK	0xC0


/* 8250 LCR - Line Control Register bits */
#define	LCR5B		0x00	/* 5 bit words */
#define	LCR6B		0x01	/* 6 bit words */
#define	LCR7B		0x02	/* 7 bit words */
#define	LCR8B		0x03	/* 8 bit words */
#define	LCR1SB		0x00	/* stop bits: 1 */
#define	LCR2SB		0x04	/* stop bits: 1.5 (for 5) or 2 */
#define	LCRPEN		0x08	/* Parity enable */
#define	LCREVP		0x10	/* Even parity select */
#define	LCRSP		0x20	/* Stick parity, par=~LCRSTP */
#define	LCRBRK		0x40	/* Set break */
#define	LCRDLAB		0x80	/* Divisor Latch Access Bit */


/* 8250 MCR - Modem Control Register bits */
#define	MCRDTR		0x01	/* Data Terminal Ready */
#define	MCRRTS		0x02	/* Request to Send */
#define	MCROUT1		0x04	/* misc. modem control line out 1 */
#define	MCROUT2		0x08	/* misc. modem control line out 2 */
#define	MCRLOOP		0x10	/* Loopback test mode */


/* 8250 RXR - Receive Data Register (reading clears IIR_RDA interrupt) */
/* 8250 TXR - Transmit Data Register (writing clears IIR_THRE interrupt) */


/* 8250 LSR - Line Status Register bits (reading clears IIR_RLS interrupt) */
#define	LSRRBF		0x01	/* Data ready */
#define	LSROER		0x02	/* Overrun error */
#define	LSRPER		0x04	/* Parity error */
#define	LSRFER		0x08	/* Framing error */
#define	LSRBI		0x10	/* Break interrupt */
#define LSRTBE		0x20	/* Transmitter line holding register empty */
#define LSRTSRE		0x40	/* Transmitter shift register empty */


/* 8250 MSR - Modem Status Register bits (reading clears IIR_MSTAT interrupt) */
#define	MSRDCTS		0x01	/* Delta Clear-to-Send (CTS) */
#define	MSRDDSR		0x02	/* Delta Data Set Ready (DSR) */
#define	MSRTERI		0x04	/* Trailing edge ring indicator */
#define	MSRDDCD		0x08	/* Delta Rx Line Signal Detect (DCD) */
#define	MSRCTS		0x10	/* Clear to send (CTS) */
#define	MSRDSR		0x20	/* Data set ready (DSR) */
#define	MSRRI		0x40	/* Ring indicator (RI) */
#define MSRDCD		0x80	/* Rx line signal detect (DCD) */


/* it seems kevin g1emm was not to happy about fifo's in 15550's , so: */
#define FIFO_TRIGGER_LEVEL	FIFO_SIZE_1	/* was _4 */
#define OUTPUT_FIFO_SIZE	1		/* was 16 */


/* some macros to make life easier */
#define FIFO_SETUP		(FIFO_ENABLE|FIFO_CLR_RX|FIFO_CLR_TX|FIFO_TRIGGER_LEVEL)

/* dtr and rts want to be on.  out2 and out1 may control IBM board chip interrupt enable */
#define	ON3	(MCRDTR | MCRRTS | MCROUT2 | MCROUT1 )	/* dtr, rts, out2, out1 */ 
#define	ON2	(MCRDTR | MCRRTS | MCROUT2 )		/* dtr, rts, out2, off  */
#define	ON1	(MCRDTR | MCRRTS | MCROUT1 )		/* dtr, rts, off , out1 */
#define	ON0	(MCRDTR | MCRRTS )			/* dtr, rts, off , off  */

static int on = ON3;			/* play with to see what works for you */
					/* states: out2, out1  */
					/* ------------------
					/* ON0   = off,  off   */
					/* ON1   = off,  out1  */
					/* ON2   = out2, off   */
					/* ON3   = out2, out1  */

#undef	OFF
#define	OFF	0x00					/* all modem lines off */

/* read DCD (carrier) state */
#define	comldcd(mx)		(inb(mx, MSR) & MSRDCD)

/* control modem lines */
#define	comldtr(mx, state)	outb(mx, MCR, state)

/* set RTS */
#define	setrts(mx)		outb(mx, MCR, (inb(mx, MCR) | MCRRTS))

/* clear RTS */
#define clrrts(mx)		outb(mx, MCR, (inb(mx, MCR) & ~MCRRTS))

/* enable selected interrupts */
#define	ienable(mx, intr)	outb(mx, INTEN, (inb(mx, INTEN) | intr))

/* disable selected interrupts */
#define idisable(mx, intr)	outb(mx, INTEN, (inb(mx, INTEN) & ~intr))

/* disable all interrupts */
#define idisableall(mx)		outb(mx, INTEN, 0)

/* set break */
#define	setbreak(mx)		outb(mx, LCR, (inb(mx, LCR) | LCRBRK))

/* remove break */
#define clrbreak(mx)		outb(mx, LCR, (inb(mx, LCR) & ~LCRBRK))

#define	amirequest2()		AMIGA->intreq = AINTSET | AIEINT2;
#define	amirequest6()		AMIGA->intreq = AINTSET | AIEEXTI; amienable6();
#define	amienable6()		AMIGA->intena = AINTSET | AIEEXTI;
#define	amidisable6()		AMIGA->intena = AINTCLR | AIEEXTI;


/* our globals */
static int mxopen(), mxclose(), mxwput(), mxwsrv(), mxrsrv(), getoblk();
static void mxputioc(), mxproc(), mxsrvioc(), mx_ttrstrt();
static void checkcarrier(), mxparam(), mxtint(), mxrint(), mxlint();
static unsigned char inb();
static void outb(mx_t *, int, unsigned char);


/*
** Local stuff.
*/

static	mx_t	mxx[sizeof(portaddr)/4]; /* setup for the most ports allowable */


/*
**  stream data structure definitions
*/

static struct module_info mx_minfo =
{
    0x6d78 /*'mx'*/, "mx", 0, INFPSZ, 256, 128,
};

static struct qinit mxrinit =
{
    NULL, mxrsrv, mxopen, mxclose, NULL, &mx_minfo, NULL,
};

static struct qinit mxwinit =
{
    mxwput, mxwsrv, NULL, NULL, NULL, &mx_minfo, NULL,
};

struct streamtab mxinfo =
{
    &mxrinit, &mxwinit, NULL, NULL,
};



/* One-time initialization */
static int initflag = 0;	/* non-zero if MAX-125 is configured */
static int
mxinit()
{
	int i;
	int t = 0;
	long dummy;
	mx_t *mx;

	autocon(MAX125, MAXNO, &maxbase, &dummy);	/* check for our board */

	if (maxbase == 0)
	    return 0;

	/* create chip address for all ports */
	for (i=0; i<nports; i++) {
	    mxx[i].inputglobsize = INPUTGLOBSIZE;
	    mxx[i].portbase = maxbase + portaddr[i];
	    mxx[i].head = mxx[i].tail = 0;

	    mx = &mxx[i];

	    idisableall(mx);		/* disable all interrupts */
	    comldtr(mx, OFF);		/* DTR, RTS OUT1, OUT2 off */

	    /* if we can write & read the scratch port for this chip, */
	    /* then it is active					  */
	    mx->active = 0;		/* init inactive */
	    outb(mx, LCR, 0x5a);
	    if (inb(mx, LCR) == 0x5a) {
		outb(mx, LCR, 0x25);
		if (inb(mx, LCR) == 0x25) {
		    mx->active = 1;	/* mark it active */
		    ++t;		/* indicate at least one active port found */
		}
	    }
	}

	if(t) {
	    ++initflag;			/* init complete */
	    if(intvector)
		/* en master int */
		*((unsigned char *)(mx->portbase + intvector)) = intenmask;

	    return 1;
	}

	/* no boards found! */
	return 0;
}


static int
mxopen(rq, devp, flag, sflag, credp)
struct queue *rq;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
	register int s, md;
	register mx_t *mx;


	prt_where = PRW_CONS;	/* output diag messages to sys console */

	if (sflag)			/* not a clone */
	    return EINVAL;		/* or module */

	s = spl6();

	if (!initflag)			/* if ports have not been initialized */
	    if(!mxinit())		/* setup the ports */
	    {
		splx(s);		/* restore previous interrupt level */
		return ENODEV;		/* bad init */
	    }

	if ((md = getminor(*devp)) >= nports)
	{
	    splx(s);			/* restore previous interrupt level */
	    return ENODEV;
	}

	mx = &mxx[md];			/* point to proper mx_t struct */

	if(!mx->active)			/* see if an active port */
	{
	    splx(s);			/* restore previous interrupt level */
	    return ENODEV;
	}

	mx->minornum = md;		/* place minor device number */

	{
	    register struct stroptions *sop;
	    mblk_t *mop = allocb(sizeof (struct stroptions), BPRI_MED);

	    if (!mop)
	    {
	        splx(s);
		cmn_err(CE_NOTE, "mx%d open: no mem for mop", mx->minornum);
	        return ENOMEM;
	    }

	    mop->b_datap->db_type = M_SETOPTS;
	    sop = (struct stroptions *)mop->b_wptr;
	    mop->b_wptr += sizeof (struct stroptions);
	    sop->so_flags = SO_HIWAT | SO_LOWAT | SO_ISTTY;
	    sop->so_hiwat = 512;
	    sop->so_lowat = 256;

	    putnext(rq, mop);
	}

	WR(rq)->q_ptr = rq->q_ptr = (caddr_t)mx;

        /*
        ** Hardware flow control available for all ports
        ** do hardware flow control.
        */
        if (!(mx->tty.t_state & ISOPEN))
        {
	    mx->output_mblk = NULL;
	    mx->cts = TRUE;
	    mx->tty.t_rdqp = rq;
	    mx->tty.t_dev = getminor(*devp);	/* needed for hd flow ctl */
	    mx->tty.t_line = 0;
	    mx->tty.t_iflag = 0;
	    mx->tty.t_oflag = 0;
	    mx->tty.t_lflag = 0;
	    mx->tty.t_cflag = B9600|SSPEED|CS8|CREAD|HUPCL;

	    mx->carrier = FALSE;

	    mxparam(mx, OPEN);
       }

       checkcarrier(mx);

       if (flag & (FNDELAY|FNONBLOCK))
       {
	    mx->tty.t_state |= CARR_ON;
	    if (mx->tty.t_state & WOPEN)
	    {
	        mx->tty.t_state &= ~WOPEN;
		wakeup((caddr_t)&mx->tty);
	    }
       }
       else
       {
	   while (!(mx->tty.t_state & CARR_ON))
	   {
	        mx->tty.t_state |= WOPEN;	/* Sleeping for carrier in open */
		sleep((caddr_t)&mx->tty, TTIPRI);
	   }
       }

       if (!mx->rdq)
       {
	   mx->rdq = rq;
	   mx->tty.t_state |= ISOPEN; /* In open */
	   amienable6();
	   ienable(mx, IENRBF);
       }

       splx(s);
       return (0);
}


static int
mxclose(q, oflag, credp)
struct queue *q;
int oflag;
cred_t *credp;
{
    register int s = spl6();
    register mx_t *mx = (mx_t *)q->q_ptr;
    register struct strtty *tp = &mx->tty;

    if (tp->t_state & ISOPEN)
    {
	if (!(oflag & (FNDELAY|FNONBLOCK)))
	{
	    while (WR(q)->q_first || tp->t_state & BUSY)
	    {
		getoblk(tp);

		if (WR(q)->q_first || tp->t_state & BUSY)
		{
		    tp->t_state |= TTIOW;
		    if (sleep((caddr_t)&tp->t_oflag, PZERO + 1| PCATCH))
		    {
			tp->t_state &= ~TTIOW;
			break;
		    }
		}
	    }
	}
    }

    if (mx->tty.t_cflag & HUPCL)
	mxparam(mx, CLOSE);

    tp->t_state = 0;
    tp->t_rdqp = NULL;
    mx->rdq = NULL;

    idisableall(mx);

    splx(s);

    return 0;
}


/* Process an output block. */
/* Returns nonzero if there was nothing in the queue. */

static int
getoblk(tp)
register struct strtty *tp;
{
    int s = spl6();
    struct queue *q = WR(tp->t_rdqp);
    mx_t *mx = (mx_t *)q->q_ptr;

    do
    {
	mx->bp = getq(q);

	if (!mx->bp)
	{
	    /* wakeup close write queue drain */
	    tp->t_state &= ~BUSY;

	    if (tp->t_state & TTIOW)
	    {
		tp->t_state &= ~TTIOW;
		wakeup((caddr_t)&tp->t_oflag);
	    }

	    splx(s);
	    return 1;
	}

	switch (mx->bp->b_datap->db_type)
	{
	case M_DATA:
#if 0
	    printf("mxgetoblk 0x%x: got DATA msg, %d bytes (%x %x %x)\n",
		   tp, blklen(mx->bp), mx->bp->b_rptr[0], mx->bp->b_rptr[1],
		   mx->bp->b_rptr[2]);
#endif

	    if (tp->t_state & (TTSTOP | TIMEOUT))
	    {
	        putbq(q, mx->bp);
		splx(s);
		return 0;
	    }

	    /*
	    ** queue bytes to be sent out
	    */

	    if (mx->output_mblk)
		putbq(q, mx->bp);
	    else
		mx->output_mblk = mx->bp;

	    /* if not busy */
	    if (!(tp->t_state & BUSY))
	    {
		ienable(mx, IENTBE);
		mxtint(mx);			/* kick it */
	    }
	    break;

	case M_IOCTL:
	case M_IOCDATA:
	    mxsrvioc(q, mx->bp);
	    break;

	default:
	    printf("mxgetoblk 0x%x: got an unknown message, type 0x%x\n",
		   tp, mx->bp->b_datap->db_type);
	    freemsg(mx->bp);
	    break;
	}
    } while (!(tp->t_state & BUSY));

    splx(s);
    return 0;
}


static void
delay(tp)
register struct strtty *tp;
{
    int s = spl6();

    tp->t_state &= ~TIMEOUT;

    splx(s);

    getoblk(tp);
}


static void
flush(tp, cmd)
struct strtty *tp;
int cmd;
{
    int s = spl6();

    if (cmd&FWRITE)
    {
	flushq(WR(tp->t_rdqp), FLUSHDATA);
	tp->t_state &= ~BUSY;
	if (tp->t_state & TTIOW)
	{
	    tp->t_state &= ~TTIOW;
	    wakeup((caddr_t)&tp->t_oflag);
	}

	mxproc(tp, T_WFLUSH);
    }

    if (cmd&FREAD)
    {
	flushq(tp->t_rdqp, FLUSHDATA);
	mxproc(tp, T_RFLUSH);
    }

    splx(s);

    getoblk(tp);
}


static int
mxwput(q, bp)
struct queue *q;
mblk_t *bp;
{
    int s;
    register mx_t *mx = (mx_t *)q->q_ptr;

    mx->tp = &mx->tty;

    switch (bp->b_datap->db_type)
    {
    case M_DATA:
	if (!(mx->tp->t_state & CARR_ON))
	{
	    putq(q, bp);
	    return 0;
	}

	s = spl6();

	while (bp)
	{
	    bp->b_datap->db_type = M_DATA;
	    mx->bp1 = unlinkb(bp);

	    if (blklen(bp) <= 0)
		freeb(bp);
	    else
		putq(q, bp);

	    bp = mx->bp1;
	}

	splx(s);

	if (q->q_first)
	    getoblk(mx->tp);
	break;

    case M_IOCTL:
    case M_IOCDATA:
	mxputioc(q, bp);
	if (q->q_first)
	    getoblk(mx->tp);
	break;

    case M_CTL:
	{
	    struct iocblk *iocb = (struct iocblk *)bp->b_rptr;

	    if ((blklen(bp) == sizeof *iocb) &&
		(iocb->ioc_cmd == MC_CANONQUERY))
		iocb->ioc_cmd = MC_DO_CANON;
	    else
		bp->b_datap->db_type = M_IOCNAK;

	    putnext(RD(q), bp);
	}
	break;

    case M_FLUSH:
	s = spl6();
	switch (bp->b_rptr[0])
	{
	case FLUSHRW:
	    flush(mx->tp, (FREAD|FWRITE));
	    bp->b_rptr[0] = FLUSHR;
	    putnext(RD(q), bp);
	    break;

	case FLUSHR:
	    flush(mx->tp, FREAD);
	    putnext(RD(q), bp);
	    break;

	case FLUSHW:
	    flush(mx->tp, FWRITE);
	    freemsg(bp);
	    break;
	}
	splx(s);
	break;

    case M_BREAK:
	s = spl6();
	mxproc(mx->tp, T_BREAK);
	splx(s);
	freemsg(bp);
	break;

    case M_START:
	s = spl6();
	mxproc(mx->tp, T_RESUME);
	splx(s);
	freemsg(bp);
	getoblk(mx->tp);
	break;

    case M_STOP:
	s = spl6();
	mxproc(mx->tp, T_SUSPEND);
	splx(s);
	freemsg(bp);
	break;

    case M_DELAY:
	s = spl6();
	mx->tp->t_state |= TIMEOUT;
	timeout(delay, (caddr_t)mx->tp, bp->b_rptr[0]);
	splx(s);
	freemsg(bp);
	break;

    case M_STARTI:
	/* Can't start/stop keyboard */
	freemsg(bp);
	break;

    case M_STOPI:
	/* Can't start/stop keyboard */
	freemsg(bp);
	break;

    case M_READ:
	/* <EH> Read queue is always up-to-date? */
	freemsg(bp);
	break;

    default:
	printf("mxwput: got an unknown message 0x%x\n", bp->b_datap->db_type);
	freemsg(bp);
	break;
    }
    return (0);	/* for lint */
}


/*
** This does all the work.
*/

static void
mxproc(tp, cmd)
register struct strtty *tp;
unsigned cmd;
{
    int s;
    mx_t *mx = (mx_t *)((caddr_t)tp - ((caddr_t)&mxx[0].tty - (caddr_t)&mxx[0]));

    switch (cmd)
    {
    case T_TIME:
	s = spl6();
	tp->t_state &= ~TIMEOUT;
	splx(s);
/*	amirequest6();			/* force int6 */
	ienable(mx, IENTBE);		/* if BREAK timeout expired, send some data */
	mxtint(mx);			/* kick it */
	getoblk(tp);
	break;

    case T_WFLUSH:
	if (mx->output_mblk)
	{
	    freemsg(mx->output_mblk);
	    mx->output_mblk = NULL;
	}
	/* fall through */
    case T_RESUME:			/* resume output */
	s = spl6();
	tp->t_state &= ~TTSTOP;
	splx(s);
/*	amirequest6();			/* force int6 */
	ienable(mx, IENTBE);		/* if resume after pause, send some data */
	mxtint(mx);			/* kick it */
	getoblk(tp);
	break;

    case T_SUSPEND:			/* suspend output */
	s = spl6();
	tp->t_state |= TTSTOP;
	splx(s);
	break;

    case T_RFLUSH:
	break;

    case T_BREAK:
	s = spl6();
	mxparam(mx, SETBREAK);
	tp->t_state |= TIMEOUT;
	timeout(mx_ttrstrt, (caddr_t)tp, (long)HZ/4);	/* BREAK length timeout */
	splx(s);
	getoblk(tp);
	break;
    }
}


static void
mxputioc(q, bp)
queue_t *q;
mblk_t *bp;
{
    register mx_t *mx = (mx_t *)q->q_ptr;
    register struct strtty *tp = &mx->tty;

    mx->iocbp1 = (struct iocblk *)bp->b_rptr;

    switch (mx->iocbp1->ioc_cmd)
    {
    case TCSETA:
    case TCSETS:
	if (tp->t_state & BUSY)
	    putbq(q, bp);		/* queue these for later */
	else
	    mxsrvioc(q, bp);
	return;

    case TCGETA:
    case TCGETS:			/* immediate parm retrieve */
	mxsrvioc(q, bp);
	return;

    default:
	if ((mx->iocbp1->ioc_cmd&IOCTYPE) == LDIOC)
	{
	    bp->b_datap->db_type = M_IOCACK; /* ignore LDIOC cmds */
	    freemsg(unlinkb(bp));
	    mx->iocbp1->ioc_count = 0;
	    putnext(RD(q), bp);
	    return;
	}
	break;
    }

    /* Other IOCTLs get queued... */
    if (q->q_first || (tp->t_state & BUSY))
	putbq(q, bp);
    else
	mxsrvioc(q, bp);
}


static int
mxwsrv() { return 0; }


static int
mxrsrv() { return 0; }


static void
mxparam(mx, xm)
mx_t *mx;
xmitter	xm;
{
    switch ((int)xm)
    {
    case OPEN:
	if (mx->tty.t_cflag & PARENB)
		if (mx->tty.t_cflag & PARODD)
			mx->lcrc = LCRPEN;
		else
			mx->lcrc = LCRPEN | LCREVP;

	switch (mx->tty.t_cflag & CSIZE)
	{
		case CS5:
			mx->lcrc |= LCR5B;
			break;

		case CS6:
			mx->lcrc |= LCR6B;
			break;

		case CS7:
			mx->lcrc |= LCR7B;
			break;

		case CS8:
			mx->lcrc |= LCR8B;
			break;
	}

	if (mx->tty.t_cflag & CSTOPB)
		mx->lcrc |= LCR2SB;

	mx->pss = mx->tty.t_cflag & CBAUD;

	if (mx->pss>(int)0 && mx->pss < (sizeof(speeds) / sizeof(speeds[0])))
	{
	    outb(mx, LCR, mx->lcrc | LCRDLAB);	/* enable divisor registers */
	    outb(mx, DIVLSB, speeds[mx->tty.t_cflag & CBAUD] & 0xff);
	    outb(mx, DIVMSB, (speeds[mx->tty.t_cflag & CBAUD] >> 8) & 0xff);
	}

	outb(mx, LCR, mx->lcrc); /* enable normal registers, output control bits */
	comldtr(mx, on);	/* set DTR, RTS, state of out2, out1 interrupt control */

/*	ienable(mx, IENMST);	/* jck: add only if mxpoll is intr driven */

	if (!(mx->tty.t_iflag & IXON) && (mx->tty.t_state & TTSTOP))
	    mxproc(&mx->tty, T_RESUME);
	break;

    case CLOSE:
	comldtr(mx, OFF);	/* clr DTR, RTS, OUT1, OUT2 */
	break;

    case SETBREAK:
	setbreak(mx);
    }
}


static void
mxsrvioc(q, mp)
queue_t *q;
mblk_t *mp;
{
    register mx_t *mx = (mx_t *)q->q_ptr;
    struct strtty *tp = &mx->tty;

    mx->iocbp = (struct iocblk *)mp->b_rptr;

    if (mp->b_datap->db_type == M_IOCDATA)
    {
	/* For copyin/copyout failures, just free message. */
	if (((struct copyresp *)mp->b_rptr)->cp_rval)
	{
	    freemsg(mp);
	    return;
	}

	if (!((struct copyresp *)mp->b_rptr)->cp_private)
	{
	    mp->b_datap->db_type = M_IOCACK;
	    freemsg(unlinkb(mp));
	    mx->iocbp->ioc_count = 0;
	    mx->iocbp->ioc_rval = 0;
	    mx->iocbp->ioc_error = 0;
	    putnext(RD(q), mp);
	    return;
	}
    }

#if 0
    printf("mxsrvioc: %s message, command 0x%x %d\n",
	   (mp->b_datap->db_type == M_IOCDATA ? "IOCDATA" :
	    mp->b_datap->db_type == M_IOCTL ? "IOCTL" : "??"),
	    mx->iocbp->ioc_cmd, (mx->tty.t_state & CARR_ON));
#endif

    switch (mx->iocbp->ioc_cmd)
    {
    case TCSETAF:
	{
	    flush(tp, FREAD);

	    if (!mp->b_cont)
	    {
		mx->iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		mx->iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    mx->cb = (struct termio *)mp->b_cont->b_rptr;

	    tp->t_cflag = (tp->t_cflag & 0xffff0000 | mx->cb->c_cflag);
	    tp->t_iflag = (tp->t_iflag & 0xffff0000 | mx->cb->c_iflag);
	    tp->t_oflag = (tp->t_oflag & 0xffff0000 | mx->cb->c_oflag);

	    mxparam(mx, OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    mx->bp2 = unlinkb(mp);
	    if (mx->bp2)
		freeb(mx->bp2);
	    mx->iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCSETA:
    case TCSETAW:
	{
	    if (!mp->b_cont)
	    {
		mx->iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		mx->iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    mx->cb = (struct termio *)mp->b_cont->b_rptr;

	    tp->t_cflag = (tp->t_cflag & 0xffff0000 | mx->cb->c_cflag);
	    tp->t_iflag = (tp->t_iflag & 0xffff0000 | mx->cb->c_iflag);
	    tp->t_oflag = (tp->t_oflag & 0xffff0000 | mx->cb->c_oflag);

	    mxparam(mx, OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    mx->bp2 = unlinkb(mp);
	    if (mx->bp2)
		freeb(mx->bp2);
	    mx->iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCSETSF:
	{
	    flush(tp, FREAD);

	    if (!mp->b_cont)
	    {
		mx->iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		mx->iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    mx->cbs = (struct termios *)mp->b_cont->b_rptr;

	    tp->t_cflag = mx->cbs->c_cflag;
	    tp->t_iflag = mx->cbs->c_iflag;
	    tp->t_oflag = mx->cbs->c_oflag;

	    mxparam(mx, OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    mx->bp2 = unlinkb(mp);
	    if (mx->bp2)
		freeb(mx->bp2);
	    mx->iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCSETS:
    case TCSETSW:
	{
	    if (!mp->b_cont)
	    {
		mx->iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		mx->iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    mx->cbs = (struct termios *)mp->b_cont->b_rptr;

	    tp->t_cflag = mx->cbs->c_cflag;
	    tp->t_iflag = mx->cbs->c_iflag;
	    tp->t_oflag = mx->cbs->c_oflag;

	    mxparam(mx, OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    mx->bp2 = unlinkb(mp);
	    if (mx->bp2)
		freeb(mx->bp2);
	    mx->iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCGETA:
	{	/* immediate parm retrieve */
	    if (mp->b_cont)		/* Bad user supplied parameter */
		freemsg(mp->b_cont);

	    if (!(mx->bp2 = allocb(sizeof (struct termio), BPRI_MED)))
	    {
		putbq(q, mp);
		bufcall(sizeof (struct termio), BPRI_MED, getoblk, (long)tp);
		return;
	    }

	    mp->b_cont = mx->bp2;
	    mx->cb = (struct termio *)mp->b_cont->b_rptr;

	    mx->cb->c_iflag = (unsigned short)tp->t_iflag;
	    mx->cb->c_oflag = (unsigned short)tp->t_oflag;
	    mx->cb->c_cflag = (unsigned short)tp->t_cflag;

	    mp->b_cont->b_wptr += sizeof (struct termio);
	    mp->b_datap->db_type = M_IOCACK;
	    mx->iocbp->ioc_count = sizeof (struct termio);
	    putnext(RD(q), mp);
	    break;
	}

    case TCGETS:
	{	/* immediate parm retrieve */
	    if (mp->b_cont)		/* Bad user supplied parameter */
		freemsg(mp->b_cont);

	    if (!(mx->bp2 = allocb(sizeof(struct termios), BPRI_MED)))
	    {
		putbq(q, mp);
		bufcall(sizeof(struct termios), BPRI_MED, getoblk, (long)tp);
		return;
	    }
	    mp->b_cont = mx->bp2;
	    mx->cbs = (struct termios *)mp->b_cont->b_rptr;

	    mx->cbs->c_iflag = tp->t_iflag;
	    mx->cbs->c_oflag = tp->t_oflag;
	    mx->cbs->c_cflag = tp->t_cflag;

	    mp->b_cont->b_wptr += sizeof(struct termios);
	    mp->b_datap->db_type = M_IOCACK;
	    mx->iocbp->ioc_count = sizeof(struct termios);
	    putnext(RD(q), mp);
	    break;
	}

    case TCSBRK:
	mp->b_datap->db_type = (mp->b_cont ? M_IOCACK : M_IOCNAK);
	freemsg(unlinkb(mp));
	mx->iocbp->ioc_count = 0;
	mx->iocbp->ioc_rval = 0;
	mx->iocbp->ioc_error = 0;
	putnext(RD(q), mp);
	mxproc(tp, T_BREAK);
	break;

    default:
	/* Unrecognized ioctl command */
	if (canput(RD(q)->q_next))
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    putnext(RD(q), mp);
	}
	else
	    putbq(q, mp);
	break;
    }
}


/* called at BREAK timeout expiration to restart access */
static void
mx_ttrstrt(mx)
mx_t *mx;
{
    mxparam(mx, OPEN);
    mxproc(&mx->tty, T_TIME);
}


/* check DCD state */
static void
checkcarrier(mx)
mx_t *mx;
{
    int	s;

    s = spl6();

    if (comldcd(mx) == MSRDCD)	/* DCD active (state opposit of Amigs ser port) */ 
    {
	if (!mx->carrier)
	{
	    mx->carrier = TRUE;
	    mx->tty.t_state |= CARR_ON;

	    if (mx->tty.t_state & WOPEN)
	    {
		mx->tty.t_state &= ~WOPEN;
		wakeup((caddr_t)&mx->tty);	/* indicate carrier became avtive */
	    }
	}
    }
    else			/* DCD not active */
    {
	if (mx->carrier)
	{
	    mx->carrier = FALSE;

	    if (!(mx->tty.t_state & ISOPEN))
		mx->tty.t_state &= ~CARR_ON;
	    else if (!(mx->tty.t_cflag & CLOCAL))
	    {
		flush(&mx->tty, FREAD|FWRITE);
		mx->tty.t_state &= ~CARR_ON;

		if (mx->rdq)		/* indicate carrier became inactive */
		    putctl1(mx->rdq->q_next, M_HANGUP, 0);
	    }
	}
    }

    splx(s);
}



/* interrupt entry point called fron ttrap.s code after aciab service */
/* interrupt server for int 6 */
void mxintr()
{
/* Theory is: scan all ports							*/
/*	      scan all tx ints, & mark if any found,				*/
/*	      scan all rx ints, service if any found,				*/
/*	      if any tx interrupts were sensed, kick external interrupt 2	*/

	register int i, txintflag;
	register unsigned char inter;
	register mx_t *mx;

	txintflag = 0;					/* init tx interrupt detector */

	/* service all pending rx interrupts */
	for (i=0; i<nports; i++)			/* scan all allowable ports */
	{
	    mx = &mxx[i];				/* set new global pointer */

	    if(mx->active				/* must be active port */
		&& ((inter = inb(mx, IIR)) != 1))	/* + bit 0 = 0 if intr pending */
	    {
		mx->lsr |= inb(mx, LSR);		/* get LSR, save special bits */
		mx->interrupt |= inter;			/* or in interrupt bits */

		if(inter & IIR_THRE)			/* if TBE interrupt */
		    ++txintflag;			/* mark for txintr int2 service */

		if(inter & IIR_RDA)			/* if RDA interrupt */
		    mxrint(mx);				/* service rx interrupt */
	    }
	}

	if(txintflag)			/* if a tx interrupt was detected */
	    amirequest2();		/* kick external interrupt 2 */
}

 
/* tx interrupt server keyed from external int 2 forced from mxintr() */
void mxtxintr()
{
	register int i;
	register mx_t *mx;


	if (!initflag)				/* must be initialized first */
	    return;

	for (i=0; i<nports; i++) {		/* scan all allowable ports */
	    mx = &mxx[i];			/* set new global pointer */
	    if(mx->active			/* only if this port is active */
		&& (mx->interrupt & IIR_THRE))	/* + IIR_THRE */
	    {
		mx->interrupt &= ~IIR_THRE;	/* clear the int flag */
		mxtint(mx);			/* service the interrupt */
	    }
	}
}



/* receive interrupt handler */
static void
mxrint(mx)
mx_t *mx;
{
    unsigned int nbytes = (BUFsize + mx->head - mx->tail) %BUFsize;

    unsigned char c = inb(mx, RXR);	/* get a byte from port, clr interrupt */

    if (mx->tty.t_iflag & IXON)
    {
	if (c == CSTART)
	{
	    mx->output_xoff = 0;
	    return;
	}
	if (c == CSTOP)
	{
	    mx->output_xoff = 1;
	    return;
	}

	if (mx->tty.t_iflag & IXANY)
	    mx->output_xoff = 0;
    }

    if (nbytes >= BUFsize-1)		/* receive buffer full */
    {
	idisable(mx, IENRBF);
	cmn_err(CE_NOTE, "rx buffer overflow on mx%d", mx->minornum);
	return;
    }

    if (nbytes > BUFsize-LOWwater)	/* hardware flow control */
	clrrts(mx);			/* clr RTS */

    mx->buff[mx->head] = c;		/* put char into fifo */
    mx->head = (mx->head+1) % BUFsize;
}



/* modem line change & rx buffer flush, polled from the kernel at clock rate (16.667 ms) */
void mxpoll()
{
	register int i;
	register mx_t *mx;


	if (!initflag)				/* must be initialized first */
	    return;

	for (i=0; i<nports; i++) {		/* scan all allowable ports */
	    mx = &mxx[i];			/* set new global pointer */
	    if(mx->active)			/* only if this port is active */
		mxlint(mx);
	}
}



/* mxlint senses modem line changes  &  flushes receive buffer */
static void
mxlint(mx)
mx_t *mx;
{
    if (hardware_flow_control && mx->tty.t_state&ISOPEN)
    {
	if (!mx->output_xoff && (mx->tty.t_state & TTSTOP))
	    mxproc(&mx->tty, T_RESUME);

	if (!mx->cts && (inb(mx, MSR) & MSRCTS))	/* if CTS went active */
	{
	    mx->cts = TRUE;
/*	    amirequest6();			/* force int6 */
	    ienable(mx, IENTBE);
	    mxtint(mx);				/* kick it */
	}
    }
    if (mx->tty.t_state & (WOPEN|ISOPEN))
	checkcarrier(mx);

    mx->mp = 0;
    while (mx->rdq && mx->tail != mx->head && canput(mx->rdq->q_next))
    {
	unsigned char c = mx->buff[mx->tail];	/* get char from buffer */

	if (mx->lsr & LSRBI)			/* indicates a break detected */
	{
	    mx->lsr &= ~LSRBI;			/* clear it */
	    mx->tail = (mx->tail+1) % BUFsize;

	    if (mx->tty.t_iflag & IGNBRK)
		continue;

	    if (mx->tty.t_lflag & NOFLSH)
	    {
		if (mx->mp)
		    putnext(mx->rdq, mx->mp);
	    }
	    else
	    {
		flush(&mx->tty, (FREAD|FWRITE));
		freemsg(mx->mp);
	    }
	    mx->mp = 0;

	    putctl1(mx->rdq->q_next, M_BREAK, 0);
	    if (mx->tty.t_iflag & BRKINT)
		putctl1(mx->rdq->q_next, M_SIG, SIGINT);
	}
	else
	{
	    if (mx->mp && mx->mp->b_wptr >= mx->mp->b_datap->db_lim)
	    {
		putnext(mx->rdq, mx->mp);
		mx->mp = 0;
	    }

	    if (!mx->mp)
		mx->mp = allocb(mx->inputglobsize, BPRI_HI);

	    if (!mx->mp)
		break;

	    mx->tail = (mx->tail+1) % BUFsize;


	    if ((mx->tty.t_iflag & ISTRIP) || (mx->tty.t_cflag & PARENB))
		c &= 0x7F;	/* Strip if wanted */

	    *mx->mp->b_wptr++ = c;	/* put char on write buffer */
	}

	ienable(mx, IENRBF);

	/* hardware flow cintrol */
	if ((BUFsize + mx->head - mx->tail)%BUFsize < BUFsize - HIGHwater)
	    setrts(mx);			/* set RTS */
    }

    if (mx->mp)
	putnext(mx->rdq, mx->mp);
}


/* transmit interrupt handler */
void
mxtint(mx)
mx_t *mx;
{
    mx->ch = (unsigned char)-1;

    /*
    ** Grab one byte from output message block and send it on
    ** its way.
    */

    /* NOTE: This may/will clear tx & other interrupts? */
    if (!(inb(mx, LSR) & LSRTBE))	/* look for TBE */
	return;

    if (!mx->rdq)
	return;

    if (hardware_flow_control)
    {
	if (!(inb(mx, MSR) & MSRCTS))	/* if CTS is inactive */
	{
	    mx->cts = FALSE;
	    mx->tty.t_state |= BUSY;
	    return;
	}
	else				/* else CTS is active */
	{
	    mx->tty.t_state &= ~BUSY;
	    
	    if (mx->tty.t_state & TTIOW)
	    {
		mx->tty.t_state &= ~TTIOW;
		wakeup((caddr_t)&mx->tty.t_oflag);
	    }
	}
    }

    if (mx->output_xoff && !(mx->tty.t_state & TTSTOP))
	mxproc(&mx->tty, T_SUSPEND);

    if (mx->tty.t_state & TTXON)
    {
	if (mx->tty.t_iflag & IXOFF)
	    mx->ch = CSTART;

	mx->tty.t_state &= ~TTXON;
    }
    else if (mx->tty.t_state & TTXOFF)
    {
	if (mx->tty.t_iflag & IXOFF)
	    mx->ch = CSTOP;

	mx->tty.t_state &= ~TTXOFF;
    }
    else if (mx->tty.t_state & TTSTOP)
    {
	idisable(mx, IENTBE);	/* No interrupts for a while */
	return;
    }

    if (mx->ch == (unsigned char)-1)
    {
	while (!mx->output_mblk ||
	       (mx->output_mblk->b_rptr >= mx->output_mblk->b_wptr))
	{
	    if (mx->output_mblk)
		freemsg(mx->output_mblk);

	    mx->output_mblk = getq(WR(mx->rdq));

	    if (!mx->output_mblk)
	    {
		mx->tty.t_state &= ~BUSY;

		if (mx->tty.t_state & TTIOW)
		{
		    mx->tty.t_state &= ~TTIOW;
		    wakeup((caddr_t)&mx->tty.t_oflag);
		}

		return;
	    }
	}

	mx->ch = *mx->output_mblk->b_rptr++;
    }
    outb(mx, TXR, mx->ch);		/* send a byte, clear the THRE interrupt */
    mx->tty.t_state |= BUSY;		/* make busy */
}


/* outb - port output routine */
static void outb(mx_t *mx, int p, unsigned char d)
{
	*((unsigned char *)(mx->portbase + (unsigned long)p)) = d;
}


/* inb - port input routine */
static unsigned char inb(mx_t *mx, int p)
{
	return(*((unsigned char *)(mx->portbase + (unsigned long)p)));
}

