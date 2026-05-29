/* mps6551 */
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
#include "ql6502.h"
#include "ql.h"

int qlopen(), qlclose(), qlwput(), qlwsrv(), qlrsrv();
void qlputioc(), qlproc(), qlsrvioc(), ql_ttrstrt();
void qlparam(), initialize();

/*
**  stream data structure definitions
*/

static struct module_info ql_minfo =
{
    'ql', "ql", 0, INFPSZ, 256, 128,
};

static struct qinit qlrinit =
{
    NULL, qlrsrv, qlopen, qlclose, NULL, &ql_minfo, NULL,
};

static struct qinit qlwinit =
{
    qlwput, qlwsrv, NULL, NULL, NULL, &ql_minfo, NULL,
};

struct streamtab qlinfo =
{
    &qlrinit, &qlwinit, NULL, NULL,
};

struct strtty ql_tty[NCARD][NLINE];
ql_t ql_board[NCARD][NLINE];
device_t *qladdr[NCARD];

static int
qlopen(rq, devp, flag, sflag, credp)
struct queue *rq;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    register int s, minor_device;
    register ql_t *ql;

    initialize();

    if (sflag)			/* not a clone */
	return EINVAL;		/* or module */

    minor_device = getminor(*devp);

    if ((qlcard(minor_device) >= NCARD) || (qlline(minor_device) >= NLINE))
	return ENXIO;

    ql = &ql_board[qlcard(minor_device)][qlline(minor_device)];

    if (!ql->lp)
	return ENXIO;

    s = spltty();

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

    WR(rq)->q_ptr = rq->q_ptr = (caddr_t)ql;

    if (!(ql->tp->t_state & ISOPEN))
    {
	ql->tp->t_rdqp = rq;
	ql->tp->t_dev = minor_device;
	ql->tp->t_line = 0;
	ql->tp->t_iflag = 0;
	ql->tp->t_oflag = 0;
	ql->tp->t_lflag = 0;
	ql->tp->t_cflag = B9600|SSPEED|CS8|CREAD|HUPCL;

	qlparam(ql, OPEN);
	ql->rts = TRUE;
	ql->rsync = FALSE;
	ql->lp->rsync = TRUE;

	while (!ql->rsync)
	    sleep(ql, TTIPRI);
    }

    if ((flag & (FNDELAY|FNONBLOCK)) || ql->carrier)
    {
	ql->tp->t_state |= CARR_ON;
	if (ql->tp->t_state & WOPEN)
	{
	    ql->tp->t_state &= ~WOPEN;
	    wakeup(ql);
	}
    }
    else
    {
	while (!(ql->tp->t_state & CARR_ON))
	{
	    ql->tp->t_state |= WOPEN;
	    sleep(ql, TTIPRI);
	}
    }

    ql->tp->t_state |= ISOPEN;

    splx(s);

    return 0;
}

static int
qlclose(q, oflag, credp)
struct queue *q;
int oflag;
cred_t *credp;
{
    int s;
    register ql_t *ql =  (ql_t *)q->q_ptr;
    register struct strtty *tp = ql->tp;

    s = spltty();

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

		    if (sleep((caddr_t)&tp->t_oflag, PZERO+1 | PCATCH))
		    {
			tp->t_state &= ~TTIOW;
			break;
		    }
		}
	    }
	}
    }

    if (ql->tp->t_cflag & HUPCL)
	qlparam(ql, CLOSE);

    tp->t_rdqp = NULL;
    tp->t_state = 0;		/* Just to be really sure --ford */

    splx(s);
}

static void
output(ql)
ql_t *ql;
{
    register struct strtty *tp = ql->tp;
    register queue_t *q;
    register mblk_t *bp;

    if (!tp->t_rdqp)
	return;

    q = WR(tp->t_rdqp);

    if (ql->lp->thead == ql->lp->ttail)
    {
	tp->t_state &= ~BUSY;
	
	if (tp->t_state & TTIOW)
	{
	    tp->t_state &= ~TTIOW;
	    wakeup(&tp->t_oflag);
	}
    }

#if 0
    if (tp->t_state & BUSY)
	return;
#endif

    while ((bp = getq(q)))
    {
	if (ql->lp->ttandem)
	{
	    putbq(q, bp);
	    return;
	}

	if (tp->t_state & TTXON)
	{
	    tp->t_state &= ~TTXON;
	    ql->lp->ttandem = CSTART;
	    putbq(q, bp);
	    return;
	}

	if (tp->t_state & TTXOFF)
	{
	    tp->t_state &= ~TTXOFF;
	    ql->lp->ttandem = CSTOP;
	    putbq(q, bp);
	    return;
	}

	if ((ql->lp->tflush) || (tp->t_state & TIMEOUT))
	{
	    putbq(q, bp);
	    return;
	}

	do
	{
	    while ((ql->lp->thead+1)%BUFSIZE != ql->lp->ttail &&
		   (bp->b_rptr < bp->b_wptr))
	    {
		ql->tbufp[ql->lp->thead] = *bp->b_rptr++;
		ql->lp->thead = (ql->lp->thead+1) % BUFSIZE;
		tp->t_state |= BUSY;
	    }

	    if (bp->b_rptr < bp->b_wptr)
	    {
		putbq(q, bp);	/* ql buffer full--bail */
		return;
	    }
	    else
	    {
		mblk_t *bp1 = bp;

		bp = unlinkb(bp1);
		freeb(bp1);
	    }
	} while (bp);
    }
}

/* Process an output block. */
/* Returns nonzero if there was nothing in the queue. */

static int
getoblk(tp)
register struct strtty *tp;
{
    int s = spltty();
    struct queue *q = WR(tp->t_rdqp);
    ql_t *ql = (ql_t *)q->q_ptr;
    mblk_t *bp;

    do
    {
	bp = getq(q);

	if (!bp)
	{
	    /* wakeup close write queue drain */
	    if (ql->lp->thead == ql->lp->ttail)
	    {
		tp->t_state &= ~BUSY;

		if (tp->t_state & TTIOW)
		{
		    tp->t_state &= ~TTIOW;
		    wakeup(&tp->t_oflag);
		}
	    }
	    splx(s);
	    return 1;
	}

	switch (bp->b_datap->db_type)
	{
	case M_DATA:
#if 0
	    printf("qlgetoblk 0x%x: got DATA msg, %d bytes (%x %x %x)\n",
		   tp, blklen(bp), bp->b_rptr[0], bp->b_rptr[1],
		   bp->b_rptr[2]);
#endif
	    putbq(q, bp);

	    if (tp->t_state & (TTSTOP | TIMEOUT))
	    {
		splx(s);
		return 0;
	    }

	    /*
	    ** queue bytes to be sent out
	    */
	    output(ql);
	    break;

	case M_IOCTL:
	case M_IOCDATA:
	    qlsrvioc(q, bp);
	    break;

	default:
	    printf("qlgetoblk 0x%x: got an unknown message, type 0x%x\n",
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
    struct queue *q = WR(tp->t_rdqp);
    ql_t *ql = (ql_t *)q->q_ptr;
    int s = spltty();

    if (cmd&FWRITE)
    {
	flushq(WR(tp->t_rdqp), FLUSHDATA);
	tp->t_state &= ~(BUSY|TBLOCK);
	if (tp->t_state & TTIOW)
	{
	    tp->t_state &= ~TTIOW;
	    wakeup(&tp->t_oflag);
	}

	qlproc(tp, T_WFLUSH);
    }

    if (cmd&FREAD)
    {
	flushq(tp->t_rdqp, FLUSHDATA);
	tp->t_state &= ~(TBLOCK);

	qlproc(tp, T_RFLUSH);
    }

    splx(s);

    getoblk(tp);
}

static int
qlwput(q, bp)
struct queue *q;
mblk_t *bp;
{
    int s;
    register ql_t *ql = (ql_t *)q->q_ptr;
    register struct strtty *tp = ql->tp;
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
	qlputioc(q, bp);
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
	qlparam(ql, SETBREAK);
	tp->t_state |= TIMEOUT;
	splx(s);
	timeout(ql_ttrstrt, ql, HZ/4);
	freemsg(bp);
	break;

    case M_START:
	s = spltty();
	qlproc(tp, T_RESUME);
	splx(s);
	freemsg(bp);
	getoblk(tp);
	break;

    case M_STOP:
	s = spltty();
	qlproc(tp, T_SUSPEND);
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
	printf("qlwput: got an unknown message 0x%x\n", bp->b_datap->db_type);
	freemsg(bp);
	break;
    }
}

/*
** This does all the work.
*/

static void
qlproc(tp, cmd)
register struct strtty *tp;
int cmd;
{
    int s;
    ql_t *ql = &ql_board[0][tp-ql_tty[0]];

    switch (cmd)
    {
    case T_TIME:
	s = spltty();
	qlparam(ql, OPEN);
	tp->t_state &= ~TIMEOUT;
	splx(s);
	getoblk(tp);
	break;

    case T_RESUME:		/* resume output */
	s = spltty();
	tp->t_state &= ~TTSTOP;
	ql->lp->tdisable = FALSE;
	splx(s);
	getoblk(tp);
	break;

    case T_SUSPEND:		/* suspend output */
	ql->lp->tdisable = TRUE;
	s = spltty();
	tp->t_state |= TTSTOP;
	splx(s);
	break;

    case T_BREAK:
	qlparam(ql, SETBREAK);
	s = spltty();
	tp->t_state |= TIMEOUT;
	splx(s);
	timeout(ql_ttrstrt, ql, HZ/4);
	getoblk(tp);
	break;

    case T_OUTPUT:
	break;

    case T_BLOCK:
	tp->t_state &= ~TTXON;
	tp->t_state |= TBLOCK | TTXOFF;
	break;

    case T_RFLUSH:
	if (!(tp->t_state&TBLOCK))
	    break;
	/* Fall through case */
    case T_UNBLOCK:
	tp->t_state &= ~ (TTXOFF|TBLOCK);
	tp->t_state |= TTXON;
	break;

    case T_WFLUSH:
	s = spltty();
	ql->lp->tdisable = TRUE;
	ql->lp->tflush = TRUE;
	tp->t_state &= ~TTSTOP;
	splx(s);
	break;
    }
}

static void
qlputioc(q, bp)
queue_t *q;
mblk_t *bp;
{
    register ql_t *ql =  (ql_t *)q->q_ptr;
    register struct strtty *tp = ql->tp;
    register struct iocblk *iocbp = (struct iocblk *)bp->b_rptr;
    mblk_t *bp1;

    switch (iocbp->ioc_cmd)
    {
    case TCSETA:
    case TCSETS:
	if (tp->t_state & BUSY)
	    putbq(q, bp);		/* queue these for later */
	else
	    qlsrvioc(q, bp);
	return;

    case TCGETA:
    case TCGETS:			/* immediate parm retrieve */
	qlsrvioc(q, bp);
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
	qlsrvioc(q, bp);
}

static int
qlwsrv() { return 0; }

static void
input(ql)
ql_t *ql;
{
    struct strtty *tp = ql->tp;
    queue_t *q = tp->t_rdqp;
    unsigned int i, c;
    unsigned char *p;
    mblk_t *bp;

    if (!q)
	return;

    if (!(tp->t_state & (WOPEN|ISOPEN)))
	return;

    if (tp->t_state & ISOPEN)
    {
	while ((i = ql->lp->rhead - ql->rtail) != 0)
	{
	    if (i > BUFSIZE)
		i = BUFSIZE - ql->rtail;

	    if (canput(q->q_next))
	    {
		if (!ql->rts)
		{
		    ql->rts = TRUE;
		    qlparam(ql, OPEN);
		}
	    }
	    else
	    {
		ql->rts = FALSE;
		qlparam(ql, RESETRTS);
		break;
	    }

	    bp = allocb(i, BPRI_HI);
	    if (!bp)
		break;

	    p = &ql->rbufp[ql->rtail];
	    ql->rtail = (ql->rtail+i) % BUFSIZE;

	    while (i--)
	    {
		c = *p++;

		if (tp->t_iflag & ISTRIP)
		    c &= 0x7F;

		if (tp->t_iflag & IXON)
		{
		    if (c == CSTART)
		    {
			qlproc(tp, T_RESUME);
			continue;
		    }
		    if (c == CSTOP)
		    {
			qlproc(tp, T_SUSPEND);
			continue;
		    }
		    if ((tp->t_state & TTSTOP) && (tp->t_iflag & IXANY))
			qlproc(tp, T_RESUME);
		}

		*bp->b_wptr++ = c;
	    }

	    putnext(q, bp);
	}
    }
    else
	ql->rtail = ql->lp->rhead;
}

static int
qlrsrv(q)
queue_t *q;
{
    input((ql_t *)q->q_ptr);
    return 0;
}

static void
handle_exceptions(ql)
ql_t *ql;
{
    struct strtty *tp = ql->tp;

    switch (ql->lp->rcondition)
    {
    case BREAK_DETECTED:
	if ((tp->t_state & ISOPEN) && (!(tp->t_iflag&IGNBRK)))
	{
	    if (!(tp->t_lflag & NOFLSH))
		flush(tp, (FREAD|FWRITE));

	    putctl1(tp->t_rdqp->q_next, M_BREAK, 0);

	    if (tp->t_iflag & BRKINT)
		putctl1(tp->t_rdqp->q_next, M_SIG, SIGINT);
	}
	break;

    case CARR_DETECTED:
	ql->carrier = TRUE;
	tp->t_state |= CARR_ON;
	if (tp->t_state & WOPEN)
	{
	    tp->t_state &= ~WOPEN;
	    wakeup(ql);
	}
	break;

    case RECEIVER_SYNC:
	ql->rsync = TRUE;
	wakeup(ql);
	break;

    case CARR_DROPPED:
	ql->carrier = FALSE;

	if (tp->t_state & ISOPEN)
	{
	    if (!(tp->t_cflag & CLOCAL))
	    {
		flush(tp, FREAD|FWRITE);
		tp->t_state &= ~CARR_ON;
		putctl1(tp->t_rdqp->q_next, M_HANGUP, 0);
	    }
	}
	else
	    tp->t_state &= ~CARR_ON;

	break;
    }

    ql->lp->rcondition = 0;
}

static void
qlparam(ql, xm)
ql_t *ql;
xmitter xm;
{
    struct strtty *tp;
    line_t *lp;
    int	b;
    static unsigned char baudtab[] =
    {
	0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x01, 0x06,
	0x07, 0x08, 0x09, 0x0A, 0x0C, 0x0E, 0x0F, 0x00,
    };

    tp = ql->tp;
    lp = ql->lp;
    b = baudtab[tp->t_cflag&CBAUD];

    lp->xon = tp->t_iflag&IXON? TRUE: FALSE;
    lp->control = 0x10 | b;

    switch ((int)xm)
    {
    case OPEN:
	if ((tp->t_cflag&CBAUD) != B0)
	{
	    lp->command = 0x0B;
	    break;
	}
	/* fall through */
    case CLOSE:
	lp->command = 0x00;
	break;

    case SETBREAK:
	lp->command = 0x0D;
	break;

    case RESETRTS:
	lp->command &= ~0xC;	/* Stop sending */
#if 0				/* just to remind me of states that could be */
	lp->command |= 0x8;	/* rts assert= normal state-- start ending */
	lp->command |= 0x4;	/* illeagal  */
	lp->command |= 0xC;	/* rts asserted and break is in action. */
#endif
	break;
    }

    if (tp->t_cflag & PARENB)
    {
	if (tp->t_cflag & PARODD)
	    lp->command |= 0x20;
	else
	    lp->command |= 0x60;
    }

    switch (tp->t_cflag & CSIZE)
    {
    case CS5:
	lp->control |= 0x60;
	break;
    case CS6:
	lp->control |= 0x40;
	break;
    case CS7:
	lp->control |= 0x20;
    }

    lp->setparams = TRUE;
}

static void
qlsrvioc(q, mp)
queue_t *q;
mblk_t *mp;
{
    ql_t *ql = (ql_t *)q->q_ptr;
    struct strtty *tp = ql->tp;
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
    printf("qlsrvioc: %s message, command 0x%x\n",
	   (mp->b_datap->db_type == M_IOCDATA ? "IOCDATA" :
	    mp->b_datap->db_type == M_IOCTL ? "IOCTL" : "??"),
	    iocbp->ioc_cmd);
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

	    qlparam(ql, OPEN);

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

	    qlparam(ql, OPEN);

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

	    qlparam(ql, OPEN);

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

	    qlparam(ql, OPEN);

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
	qlproc(tp, T_BREAK);
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
ql_ttrstrt(ql)
ql_t *ql;
{
    qlparam(ql, OPEN);
    ql->tp->t_state &= ~TIMEOUT;
}

static void
newcard(dp)
device_t *dp;
{
    device_t **pp;

    pp = (device_t **)endof(qladdr) - 1;

    if (pp[0])
    {
	printf("ql: too many cards\n");
	return;
    }

    while ((pp > qladdr) && (! pp[-1] || pp[-1] > dp))
    {
	pp[0] = pp[-1];
	--pp;
    }

    pp[0] = dp;
}

static void
startcard(dp)
device_t *dp;
{
    unsigned char *ip, *op;

    ip = ql6502code;
    op = (unsigned char *)dp;

    while (ip < (unsigned char *)endof(ql6502code))
    {
	short s = *(short *)ip;

	ip += sizeof(s);

	if (s < 0)
	{
	    while (s++)
		*op++ = '\0';
	}
	else
	{
	    while (s--)
		*op++ = *ip++;
	}
    }

    dp->start = 0;
}

static void
initialize()
{
    device_t *dp;
    unsigned int c, l;
    unsigned char *x;
    int s;
    static int done;

    if (done)
	return;

    done = TRUE;

    s = spltty();

    for (c=0; autocon(0x02020046, c, &dp, &x); ++c)
	newcard(dp);

    for (c=0; autocon(0x02020045, c, &dp, &x); ++c)
	newcard(dp);

    c = 0;

    while ((c < nel(qladdr)) && (dp = qladdr[c]))
    {
	startcard(dp);

	for (l=0; l<NLINE; ++l)
	{
	    ql_board[c][l].tp = &ql_tty[c][l];
	    ql_board[c][l].lp = &dp->line[l];
	    ql_board[c][l].tbufp = dp->tbuf[l];
	    ql_board[c][l].rbufp = dp->rbuf[l];
	}
	++c;
    }

    splx(s);
}

void
qlintr()
{
    unsigned int c, l;

    for (c=0; (c < NCARD) && (qladdr[c]); ++c)
    {
	qladdr[c]->irqack = 0;	/* .. just in case--rico */

	for (l=0; l < NLINE; ++l)
	{
	    handle_exceptions(&ql_board[c][l]);
	    input(&ql_board[c][l]);
	    output(&ql_board[c][l]);
	}
    }
}
