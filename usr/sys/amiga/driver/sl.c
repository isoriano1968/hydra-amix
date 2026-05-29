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
#include "amigahr.h"
#include "sl.h"


/* Amiga serdatr register: */
#define DATABITS 0x0FF
#define STOPBIT  0x100
#define DATASTOP 0x1FF

/* Amiga adkcon register: */
#define UARTBRK (1<<11)


int slopen(), slclose(), slwput(), slwsrv(), slrsrv();
void slputioc(), slproc(), slsrvioc(), sl_ttrstrt();
void checkcarrier(), slparam(), sltint();

/*
** Local stuff.
*/
sl_t sl = { INPUTGLOBSIZE, EVERYSOOFTEN, };
static int tictoc;

/*
**  stream data structure definitions
*/

static struct module_info sl_minfo =
{
    'sl', "sl", 0, INFPSZ, 256, 128,
};

static struct qinit slrinit =
{
    NULL, slrsrv, slopen, slclose, NULL, &sl_minfo, NULL,
};

static struct qinit slwinit =
{
    slwput, slwsrv, NULL, NULL, NULL, &sl_minfo, NULL,
};

struct streamtab slinfo =
{
    &slrinit, &slwinit, NULL, NULL,
};

/* One-time initialization */
static int initflag;
void
slinit()
{
    ACIAB->ddra |= (DTR|RTS);	/* Outputs */
    ACIAB->ddra &= ~(CD|CTS);	/* Inputs */
    idisable(AIESRBF);
    idisable(AIESTBE);
    ++initflag;
}

static int
slopen(rq, devp, flag, sflag, credp)
struct queue *rq;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    register int s, minor_device;

    if (sflag)			/* not a clone */
	return EINVAL;		/* or module */

    s = spltty();

    if (!initflag)
	slinit();

    {
	register struct stroptions *sop;
	mblk_t *mop = allocb(sizeof (struct stroptions), BPRI_MED);

	if (!mop)
	{
	    splx(s);
	    return EAGAIN;
	}

	mop->b_datap->db_type = M_SETOPTS;
	sop = (struct stroptions *)mop->b_wptr;
	mop->b_wptr += sizeof (struct stroptions);
	sop->so_flags = SO_HIWAT | SO_LOWAT | SO_ISTTY;
	sop->so_hiwat = 512;
	sop->so_lowat = 256;

	putnext(rq, mop);
    }

    /*
    ** Hardware flow control on if minor == 0, otherwise don't
    ** do hardware flow control.
    */
    if (!(sl.tty.t_state & ISOPEN))
    {
	sl.output_mblk = NULL;
	sl.cts = TRUE;
	sl.tty.t_rdqp = rq;
	sl.tty.t_dev = getminor(*devp);	/* needed for hd flow ctl */
	sl.tty.t_line = 0;
	sl.tty.t_iflag = 0;
	sl.tty.t_oflag = 0;
	sl.tty.t_lflag = 0;
	sl.tty.t_cflag = B9600|SSPEED|CS8|CREAD|HUPCL;

	sl.carrier = FALSE;

	slparam(OPEN);
    }

    checkcarrier();

    if (flag & (FNDELAY|FNONBLOCK))
    {
	sl.tty.t_state |= CARR_ON;
	if (sl.tty.t_state & WOPEN)
	{
	    sl.tty.t_state &= ~WOPEN;
	    wakeup(&sl.tty);
	}
    }
    else
    {
	while (!(sl.tty.t_state & CARR_ON))
	{
	    sl.tty.t_state |= WOPEN;	/* Sleeping for carrier in open */
	    sleep(&sl.tty, TTIPRI);
	}
    }

    if (!sl.rdq)
    {
	sl.rdq = rq;
	sl.tty.t_state |= ISOPEN; /* In open */
	ienable(AIESRBF);
    }

    splx(s);

    return 0;
}

static int
slclose(q, oflag, credp)
struct queue *q;
int oflag;
cred_t *credp;
{
    register int s = spltty();
    register struct strtty *tp = &sl.tty;

    if (tp->t_state & ISOPEN)
    {
	int cflag = tp->t_cflag;

	if (!(oflag & (FNDELAY|FNONBLOCK)))
	{
	    while (WR(q)->q_first || tp->t_state & BUSY)
	    {
		(void) getoblk(tp);

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

    if (sl.tty.t_cflag & HUPCL)
	slparam(CLOSE);

    tp->t_state = 0;
    tp->t_rdqp = NULL;
    sl.rdq = NULL;

    idisable(AIESRBF);
    idisable(AIESTBE);

    splx(s);

    return 0;
}

/* Process an output block. */
/* Returns nonzero if there was nothing in the queue. */

static int
getoblk(tp)
register struct strtty *tp;
{
    int s = spltty();
    struct queue *q = WR(tp->t_rdqp);
    mblk_t *bp;

    do
    {
	bp = getq(q);

	if (!bp)
	{
	    /* wakeup close write queue drain */
	    tp->t_state &= ~BUSY;

	    if (tp->t_state & TTIOW)
	    {
		tp->t_state &= ~TTIOW;
		wakeup(&tp->t_oflag);
	    }

	    splx(s);
	    return 1;
	}

	switch (bp->b_datap->db_type)
	{
	case M_DATA:
#if 0
	    printf("slgetoblk 0x%x: got DATA msg, %d bytes (%x %x %x)\n",
		   tp, blklen(bp), bp->b_rptr[0], bp->b_rptr[1],
		   bp->b_rptr[2]);
#endif
	    if (tp->t_state & (TTSTOP | TIMEOUT))
	    {
		putbq(q, bp);
		splx(s);
		return 0;
	    }

	    /*
	    ** queue bytes to be sent out
	    */

	    if (sl.output_mblk)
		putbq(q, bp);
	    else
		sl.output_mblk = bp;

	    if (!(tp->t_state & BUSY))
	    {
		ienable(AIESTBE);
		sltint();
	    }
	    break;

	case M_IOCTL:
	case M_IOCDATA:
	    slsrvioc(q, bp);
	    break;

	default:
	    printf("slgetoblk 0x%x: got an unknown message, type 0x%x\n",
		   tp, bp->b_datap->db_type);
	    freemsg(bp);
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
    int s = spltty();

    tp->t_state &= ~TIMEOUT;

    splx(s);

    getoblk(tp);
}

static void
flush(tp, cmd)
struct strtty *tp;
int cmd;
{
    int s = spltty();

    if (cmd&FWRITE)
    {
	flushq(WR(tp->t_rdqp), FLUSHDATA);
	tp->t_state &= ~BUSY;

	if (tp->t_state & TTIOW)
	{
	    tp->t_state &= ~TTIOW;
	    wakeup(&tp->t_oflag);
	}

	slproc(tp, T_WFLUSH);
    }

    if (cmd&FREAD)
    {
	flushq(tp->t_rdqp, FLUSHDATA);

	slproc(tp, T_RFLUSH);
    }

    splx(s);
    getoblk(tp);
}

static int
slwput(q, bp)
struct queue *q;
mblk_t *bp;
{
    int s;
    struct strtty *tp = &sl.tty;
    mblk_t *bp1;

    switch (bp->b_datap->db_type)
    {
    case M_DATA:
	if (!(tp->t_state & CARR_ON))
	{
	    putq(q, bp);
	    return 0;
	}

	s = spltty();

	while (bp)
	{
	    bp->b_datap->db_type = M_DATA;
	    bp1 = unlinkb(bp);

	    if (blklen(bp) <= 0)
		freeb(bp);
	    else
		putq(q, bp);

	    bp = bp1;
	}

	splx(s);

	if (q->q_first)
	    getoblk(tp);
	break;

    case M_IOCTL:
    case M_IOCDATA:
	slputioc(q, bp);
	if (q->q_first)
	    getoblk(tp);
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
	s = spltty();
	switch (bp->b_rptr[0])
	{
	case FLUSHRW:
	    flush(tp, (FREAD|FWRITE));
	    bp->b_rptr[0] = FLUSHR;
	    putnext(RD(q), bp);
	    break;

	case FLUSHR:
	    flush(tp, FREAD);
	    putnext(RD(q), bp);
	    break;

	case FLUSHW:
	    flush(tp, FWRITE);
	    freemsg(bp);
	    break;
	}
	splx(s);
	break;

    case M_BREAK:
	s = spltty();
	slproc(tp, T_BREAK);
	splx(s);
	freemsg(bp);
	break;

    case M_START:
	s = spltty();
	slproc(tp, T_RESUME);
	splx(s);
	freemsg(bp);
	getoblk(tp);
	break;

    case M_STOP:
	s = spltty();
	slproc(tp, T_SUSPEND);
	splx(s);
	freemsg(bp);
	break;

    case M_DELAY:
	s = spltty();
	tp->t_state |= TIMEOUT;
	timeout(delay, (caddr_t)tp, bp->b_rptr[0]);
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
	printf("slwput: got an unknown message 0x%x\n", bp->b_datap->db_type);
	freemsg(bp);
	break;
    }
}

/*
** This does all the work.
*/

static void
slproc(tp, cmd)
register struct strtty *tp;
unsigned cmd;
{
    int s;

    switch (cmd)
    {
    case T_TIME:
	s = spltty();
	tp->t_state &= ~TIMEOUT;
	splx(s);
	irequest(AIESTBE);
	getoblk(tp);
	break;

    case T_WFLUSH:
	if (sl.output_mblk)
	{
	    freemsg(sl.output_mblk);
	    sl.output_mblk = NULL;
	}
	/* fall through */
    case T_RESUME:			/* resume output */
	s = spltty();
	tp->t_state &= ~TTSTOP;
	splx(s);
	irequest(AIESTBE);
	getoblk(tp);
	break;

    case T_SUSPEND:			/* suspend output */
	s = spltty();
	tp->t_state |= TTSTOP;
	splx(s);
	break;

    case T_RFLUSH:
	break;

    case T_BREAK:
	s = spltty();
	slparam(SETBREAK);
	tp->t_state |= TIMEOUT;
	timeout(sl_ttrstrt, tp, HZ/4);
	splx(s);
	getoblk(tp);
	break;
    }
}

static void
slputioc(q, bp)
queue_t *q;
mblk_t *bp;
{
    struct iocblk *iocbp = (struct iocblk *)bp->b_rptr;
    struct strtty *tp = &sl.tty;
    mblk_t *bp1;

    switch (iocbp->ioc_cmd)
    {
    case TCSETA:
    case TCSETS:
	if (tp->t_state & BUSY)
	    putbq(q, bp);		/* queue these for later */
	else
	    slsrvioc(q, bp);
	return;

    case TCGETA:
    case TCGETS:			/* immediate parm retrieve */
	slsrvioc(q, bp);
	return;

    default:
	if ((iocbp->ioc_cmd&IOCTYPE) == LDIOC)
	{
	    bp->b_datap->db_type = M_IOCACK; /* ignore LDIOC cmds */
	    freemsg(unlinkb(bp));
	    iocbp->ioc_count = 0;
	    putnext(RD(q), bp);
	    return;
	}
	break;
    }

    /* Other IOCTLs get queued... */
    if (q->q_first || (tp->t_state & BUSY))
	putbq(q, bp);
    else
	slsrvioc(q, bp);
}

static int
slwsrv() { return 0; }

static int
slrsrv() { return 0; }

static void
slparam(xm)
xmitter	xm;
{
    unsigned int s;
    static unsigned short speeds[] =
    {
	0,			/* hangup */
	31 - 1,			/* 115200 for ql (hardware can't do 50) */
	62 - 1,			/* 57600 is popular (hardware can't do 75) */
	32542 - 1,		/* 110 */
	26714 - 1,		/* 134 */
	23864 - 1,		/* 150 */
	115 - 1,		/* 31250 is for MIDI */
	11932 - 1,		/* 300 */
	5966 - 1,		/* 600 */
	2983 - 1,		/* 1200 */
	1989 - 1,		/* 1800 */
	1492 - 1,		/* 2400 */
	746 - 1,		/* 4800 */
	373 - 1,		/* 9600 */
	186 - 1,		/* 19200 */
	93 - 1			/* 38400 */
    };

    switch ((int)xm)
    {
    case OPEN:
	s = sl.tty.t_cflag & CBAUD;

	if (s>0 && s < (sizeof(speeds) / sizeof(speeds[0])))
	{
	    AMIGA->serper = DATA8 | speeds[s];
	    ACIAB->pra &= ~DTR;
	}
	else
	    ACIAB->pra |= DTR;

	AMIGA->adkcon = ADKCLR | UARTBRK;

	if (!(sl.tty.t_iflag & IXON) && (sl.tty.t_state & TTSTOP))
	    slproc(&sl.tty, T_RESUME);
	break;

    case CLOSE:
	ACIAB->pra |= DTR;
	break;

    case SETBREAK:
	AMIGA->adkcon = ADKSET | UARTBRK;
    }
}

static void
slsrvioc(q, mp)
queue_t *q;
mblk_t *mp;
{
    struct strtty *tp = &sl.tty;
    struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;
    int error;
    mblk_t *bp1;

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
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = 0;
	    putnext(RD(q), mp);
	    return;
	}
    }

#if 0
    printf("slsrvioc: %s message, command 0x%x %d\n",
	   (mp->b_datap->db_type == M_IOCDATA ? "IOCDATA" :
	    mp->b_datap->db_type == M_IOCTL ? "IOCTL" : "??"),
	    iocbp->ioc_cmd, (sl.tty.t_state & CARR_ON));
#endif

    switch (iocbp->ioc_cmd)
    {
    case TCSETAF:
	{
	    register struct termio *cb;

	    flush(tp, FREAD);

	    if (!mp->b_cont)
	    {
		iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    cb = (struct termio *)mp->b_cont->b_rptr;

	    tp->t_cflag = (tp->t_cflag & 0xffff0000 | cb->c_cflag);
	    tp->t_iflag = (tp->t_iflag & 0xffff0000 | cb->c_iflag);
	    tp->t_oflag = (tp->t_oflag & 0xffff0000 | cb->c_oflag);

	    slparam(OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    bp1 = unlinkb(mp);
	    if (bp1)
		freeb(bp1);
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCSETA:
    case TCSETAW:
	{
	    register struct termio *cb;

	    if (!mp->b_cont)
	    {
		iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    cb = (struct termio *)mp->b_cont->b_rptr;

	    tp->t_cflag = (tp->t_cflag & 0xffff0000 | cb->c_cflag);
	    tp->t_iflag = (tp->t_iflag & 0xffff0000 | cb->c_iflag);
	    tp->t_oflag = (tp->t_oflag & 0xffff0000 | cb->c_oflag);

	    slparam(OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    bp1 = unlinkb(mp);
	    if (bp1)
		freeb(bp1);
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCSETSF:
	{
	    register struct termios *cb;

	    flush(tp, FREAD);

	    if (!mp->b_cont)
	    {
		iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    cb = (struct termios *)mp->b_cont->b_rptr;

	    tp->t_cflag = cb->c_cflag;
	    tp->t_iflag = cb->c_iflag;
	    tp->t_oflag = cb->c_oflag;

	    slparam(OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    bp1 = unlinkb(mp);
	    if (bp1)
		freeb(bp1);
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCSETS:
    case TCSETSW:
	{
	    register struct termios *cb;

	    if (!mp->b_cont)
	    {
		iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }
	    cb = (struct termios *)mp->b_cont->b_rptr;

	    tp->t_cflag = cb->c_cflag;
	    tp->t_iflag = cb->c_iflag;
	    tp->t_oflag = cb->c_oflag;

	    slparam(OPEN);

	    mp->b_datap->db_type = M_IOCACK;
	    bp1 = unlinkb(mp);
	    if (bp1)
		freeb(bp1);
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case TCGETA:
	{	/* immediate parm retrieve */
	    register struct termio *cb;

	    if (mp->b_cont)		/* Bad user supplied parameter */
		freemsg(mp->b_cont);

	    if (!(bp1 = allocb(sizeof (struct termio), BPRI_MED)))
	    {
		putbq(q, mp);
		bufcall(sizeof (struct termio), BPRI_MED, getoblk, (long)tp);
		return;
	    }

	    mp->b_cont = bp1;
	    cb = (struct termio *)mp->b_cont->b_rptr;

	    cb->c_iflag = (unsigned short)tp->t_iflag;
	    cb->c_oflag = (unsigned short)tp->t_oflag;
	    cb->c_cflag = (unsigned short)tp->t_cflag;

	    mp->b_cont->b_wptr += sizeof (struct termio);
	    mp->b_datap->db_type = M_IOCACK;
	    iocbp->ioc_count = sizeof (struct termio);
	    putnext(RD(q), mp);
	    break;
	}

    case TCGETS:
	{	/* immediate parm retrieve */
	    register struct termios *cb;

	    if (mp->b_cont)		/* Bad user supplied parameter */
		freemsg(mp->b_cont);

	    if (!(bp1 = allocb(sizeof(struct termios), BPRI_MED)))
	    {
		putbq(q, mp);
		bufcall(sizeof(struct termios), BPRI_MED, getoblk, (long)tp);
		return;
	    }
	    mp->b_cont = bp1;
	    cb = (struct termios *)mp->b_cont->b_rptr;

	    cb->c_iflag = tp->t_iflag;
	    cb->c_oflag = tp->t_oflag;
	    cb->c_cflag = tp->t_cflag;

	    mp->b_cont->b_wptr += sizeof(struct termios);
	    mp->b_datap->db_type = M_IOCACK;
	    iocbp->ioc_count = sizeof(struct termios);
	    putnext(RD(q), mp);
	    break;
	}

    case TCSBRK:
	mp->b_datap->db_type = (mp->b_cont ? M_IOCACK : M_IOCNAK);
	freemsg(unlinkb(mp));
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = 0;
	putnext(RD(q), mp);
	slproc(tp, T_BREAK);
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

static void
sl_ttrstrt(tp)
struct strtty *tp;
{
    slparam(OPEN);
    slproc(tp, T_TIME);
}

static void
checkcarrier()
{
    int	s;

    s = spltty();

    if ((ACIAB->pra&CD) == 0)
    {
	if (!sl.carrier)
	{
	    sl.carrier = TRUE;
	    sl.tty.t_state |= CARR_ON;

	    if (sl.tty.t_state & WOPEN)
	    {
		sl.tty.t_state &= ~WOPEN;
		wakeup(&sl.tty);
	    }
	}
    }
    else
    {
	if (sl.carrier)
	{
	    sl.carrier = FALSE;

	    if (tictoc)
		untimeout(tictoc);

	    if (!(sl.tty.t_state & ISOPEN))
		sl.tty.t_state &= ~CARR_ON;
	    else if (!(sl.tty.t_cflag & CLOCAL))
	    {
		flush(&sl.tty, FREAD|FWRITE);
		sl.tty.t_state &= ~CARR_ON;

		if (sl.rdq)
		    putctl1(sl.rdq->q_next, M_HANGUP, 0);
	    }
	}
    }

    splx(s);
}


#define BUFsize   256U
#define HIGHwater 64
#define LOWwater  32
static volatile	unsigned int head, tail;
static unsigned short buff[BUFsize];


/* slrint runs from level 5 hardware interrupt */
void
slrint()
{
    unsigned int nbytes = (BUFsize+head-tail)%BUFsize;
    unsigned short c = AMIGA->serdatr & DATASTOP;

    if (sl.tty.t_iflag & IXON)
    {
	if (c == (CSTART|STOPBIT))
	{
	    sl.output_xoff = 0;
	    return;
	}
	if (c == (CSTOP|STOPBIT))
	{
	    sl.output_xoff = 1;
	    return;
	}

	if (sl.tty.t_iflag & IXANY)
	    sl.output_xoff = 0;
    }

    if (nbytes >= BUFsize-1)
    {
	idisable(AIESRBF);
	return;
    }

    if (nbytes > BUFsize-LOWwater)
	ACIAB->pra |= RTS;	/* RTS off */

    buff[head] = c;
    head = (head+1) % BUFsize;
}

/* slpoll runs at clock priority */
void
slpoll()
{
    mblk_t *mp;

    if (hardware_flow_control && sl.tty.t_state&ISOPEN)
    {
	if (!sl.output_xoff && (sl.tty.t_state & TTSTOP))
	    slproc(&sl.tty, T_RESUME);

	if (!sl.cts && !(ACIAB->pra&CTS))
	{
	    sl.cts = TRUE;
	    irequest(AIESTBE);	/* Try to send another byte */
	}
    }
    if (sl.tty.t_state & (WOPEN|ISOPEN))
	checkcarrier();

    mp = 0;
    while (sl.rdq && tail != head && canput(sl.rdq->q_next))
    {
	unsigned short c = buff[tail];

	if (!(c & DATASTOP))	/* No bits indicates a break */
	{
	    tail = (tail+1) % BUFsize;

	    if (sl.tty.t_iflag & IGNBRK)
		continue;

	    if (sl.tty.t_lflag & NOFLSH)
	    {
		if (mp)
		    putnext(sl.rdq, mp);
	    }
	    else
	    {
		flush(&sl.tty, (FREAD|FWRITE));
		freemsg(mp);
	    }
	    mp = 0;

	    putctl1(sl.rdq->q_next, M_BREAK, 0);
	    if (sl.tty.t_iflag & BRKINT)
		putctl1(sl.rdq->q_next, M_SIG, SIGINT);
	}
	else
	{
	    if (mp && mp->b_wptr >= mp->b_datap->db_lim)
	    {
		putnext(sl.rdq, mp);
		mp = 0;
	    }

	    if (!mp)
		mp = allocb(sl.inputglobsize, BPRI_HI);

	    if (!mp)
		break;

	    tail = (tail+1) % BUFsize;

	    c &= DATABITS;	/* Get the real character */

	    if ((sl.tty.t_iflag & ISTRIP) || (sl.tty.t_cflag & PARENB))
		c &= 0x7F;	/* Strip if wanted */

	    *mp->b_wptr++ = c;
	}

	ienable(AIESRBF);
	if ((BUFsize+head-tail)%BUFsize < BUFsize-HIGHwater)
	    ACIAB->pra &= ~RTS;	/* RTS on */
    }

    if (mp)
	putnext(sl.rdq, mp);
}


void
sltint()
{
    unsigned int ch = (unsigned int)-1;
    extern unsigned char partab[];

    /*
    ** Grab one byte from output message block and send it on
    ** its way.
    */

    if (!(AMIGA->serdatr&TBE))
	return;

    if (!sl.rdq)
	return;

    if (hardware_flow_control)
    {
	if (ACIAB->pra&CTS)		/* Not Clear To Send? */
	{
	    sl.cts = FALSE;
	    sl.tty.t_state |= BUSY;
	    return;
	}
	else
	{
	    sl.tty.t_state &= ~BUSY;
	    
	    if (sl.tty.t_state & TTIOW)
	    {
		sl.tty.t_state &= ~TTIOW;
		wakeup(&sl.tty.t_oflag);
	    }
	}
    }

    if (sl.output_xoff && !(sl.tty.t_state & TTSTOP))
	slproc(&sl.tty, T_SUSPEND);

    if (sl.tty.t_state & TTXON)
    {
	if (sl.tty.t_iflag & IXOFF)
	    ch = CSTART;

	sl.tty.t_state &= ~TTXON;
    }
    else if (sl.tty.t_state & TTXOFF)
    {
	if (sl.tty.t_iflag & IXOFF)
	    ch = CSTOP;

	sl.tty.t_state &= ~TTXOFF;
    }
    else if (sl.tty.t_state & TTSTOP)
    {
	idisable(AIESTBE);	/* No interrupts for a while */
	return;
    }

    if (ch == (unsigned int)-1)
    {
	while (!sl.output_mblk ||
	       (sl.output_mblk->b_rptr >= sl.output_mblk->b_wptr))
	{
	    if (sl.output_mblk)
		freemsg(sl.output_mblk);

	    sl.output_mblk = getq(WR(sl.rdq));

	    if (!sl.output_mblk)
	    {
		sl.tty.t_state &= ~BUSY;

		if (sl.tty.t_state & TTIOW)
		{
		    sl.tty.t_state &= ~TTIOW;
		    wakeup(&sl.tty.t_oflag);
		}

		return;
	    }
	}

	ch = *sl.output_mblk->b_rptr++;
    }

    if (sl.tty.t_cflag & PARENB)
    {
	ch &= 0x7F;
	ch |= partab[ch] & 0x80;
	if (sl.tty.t_cflag & PARODD)
	    ch ^= 0x80;
    }

    AMIGA->serdat = STOPBIT | ch; /* Or'd stop bit in */

    sl.tty.t_state |= BUSY;
}
