/*
 * A streams driver for Amiga "console screens" using the "screens" facility.
 * Patchlevel: c0.c_1.1
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
#include "amigahr.h"
#include "memory.h"
#include "screen.h"
#include "console.h"

extern void figuredisplaytype();
extern void conscop(), consvb();
extern void revchar();
extern void ahrinit();

#ifdef DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#define bitsof(x) ((sizeof (x))*8)
#define PLANESIZE(cp) ((cp)->bitmap.height*(cp)->bitmap.width/8)

#define READBUFSZ 16

static void coinit();
static void coproc(), coparam(), flush(), putioc(), delay(), srvioc();
static int consinit(), getoblk();
static mblk_t *get_buffer();

static struct strtty co_tty[MAXCONSOLES];
static struct console consoles[MAXCONSOLES];

static int coopen(), coclose(), wput(), wsrv(), rsrv();

static struct module_info co_info = { 42, "co", 0, INFPSZ, 256, 128, };
static struct qinit co_rint = { putq, rsrv, coopen, coclose, NULL, &co_info, NULL, };
static struct qinit co_wint = { wput, wsrv, NULL, NULL, NULL, &co_info, NULL, };
struct streamtab coinfo = { &co_rint, &co_wint, NULL, NULL, };

static char coinitflag;


/* One-time startup initialization for the console driver */
static void coinit()
{
    if (coinitflag)
	return;
    ++coinitflag;

    if (!displaytype)
	figuredisplaytype();

    ahrinit();
}


static void consmisc(sp, cmd)
struct screen *sp;
int cmd;
{
    register struct console *cp = (struct console *)sp->user;
    switch (cmd)
    {
    case SC_CHANGE:
	if ((sp->flags & Sf_DISPLAY) &&
	    !(cp->scrflags & Sf_DISPLAY) &&
	    (sp->flags & Sf_NEEDCOP))
	    conscop(sp);
	cp->scrflags = sp->flags;
	break;
    case SC_VBCALL:
	consvb(sp);
	break;
    case SC_BLDCOP:
	conscop(sp);
	break;
    }
}


static void conskb(sp, code)
struct screen *sp;
unsigned long code;
{
    register struct console *cp = (struct console *)sp->user;
    register unsigned char qualifiers = (unsigned char)(code>>8);
    register unsigned char c = (unsigned char)code;
    mblk_t *bp;

    if (c != code)
    {
#ifdef DEBUG
	printf("conskb(0x%x) ", code);
#endif
	return;				/* Ignore special functions for now. */
    }

    /* We shouldn't get here unless the device is open, */
    /* but check just in case. */
    if (!cp->tty ||
	!cp->tty->t_rdqp)
	return;

    if (cp->tty->t_iflag&IXON)
    {
	if (c == CSTART)
	{
	    coproc(cp->tty, T_RESUME);
	    return;
	}
	if (c == CSTOP)
	{
	    coproc(cp->tty, T_SUSPEND);
	    return;
	}
	if ((cp->tty->t_state&TTSTOP) && (cp->tty->t_iflag&IXANY))
	    coproc(cp->tty, T_RESUME);
    }

    if (bp = get_buffer(cp->tty, 1))
    {
	*bp->b_wptr++ = c;
	++cp->tty->t_in.bu_cnt;
	/* For a real device, perhaps this should be */
	/* deferred to a periodic poll: */
	putq(cp->tty->t_rdqp, bp); /* Send up whatever we got */
	cp->tty->t_in.bu_bp = NULL;
	(void)get_buffer(cp->tty, 0);
    }
}


/* Get an input buffer with at least nbytes bytes */
static mblk_t *get_buffer(tp, nbytes)
struct strtty *tp;
unsigned int nbytes;
{
    register mblk_t *bp = tp->t_in.bu_bp;

    if (bp && nbytes >= bp->b_datap->db_lim - bp->b_wptr)
    {
	if (canput(tp->t_rdqp))
	    putq(tp->t_rdqp, bp);
	else
	    freeb(bp);
	bp = tp->t_in.bu_bp = NULL;
    }

    if (!bp)
    {
	bp = allocb(min(nbytes, READBUFSZ), BPRI_HI);
	if (bp)
	{
	    tp->t_in.bu_bp = bp;
	    tp->t_in.bu_cnt = 0;
	}
	else
	    printf("console_get_buffer: out of blocks\n");
    }

    return bp;
}


/*
 * Read service procedure.  Pass everything upstream.
 */
static int rsrv(q)
queue_t *q;
{
    register mblk_t *bp;
    register struct strtty *tp = ((struct console *)q->q_ptr)->tty;
    int s = splscr();

    while (canput(q->q_next) && (bp = getq(q)))
    {
	putnext(q, bp);
    }

    splx(s);
    return 0;
}


void convert_ucolors(sp)
struct screen *sp;
{
    unsigned int i;

    if (sp->modes & Sm_HEDLEY)
    {
#define I7(x) ((x&0x80)?1:0)
#define I6(x) ((x&0x40)?1:0)
	for ( i=0 ; i < 16 ; ++i )
	    sp->color[i] = rgbi(I7(sp->ucolor[((i&4)>>1)+((i&1)>>0)].gray),
				I7(sp->ucolor[((i&8)>>2)+((i&2)>>1)].gray),
				I6(sp->ucolor[((i&4)>>1)+((i&1)>>0)].gray),
				I6(sp->ucolor[((i&8)>>2)+((i&2)>>1)].gray));
    }
    else
    {
	static unsigned short hedley_intensities[] =
	{ 0x000, 0x001, 0x009, 0x088, 0x880, 0x881, 0x889, };
	int n=NUCOLREGS(sp);

	for ( i=0 ; i<n ; ++i )
	    if (displaytype & DT_HEDLEY)
		sp->color[i] = hedley_intensities[sp->ucolor[i].gray * 7 / 256];
	    else
		sp->color[i] = ((sp->ucolor[i].red & 0xf0)<<4 |
				(sp->ucolor[i].green & 0xf0) |
				(sp->ucolor[i].blue & 0xf0)>>4);
    }
}


int console_modes = 0;
int console_depth = 1;

/* Initialize a console unit, making it ready for display */
static int consinit(cp)
register struct console *cp;
{
    register int i;
    register struct screen *sp;

    if (!coinitflag)
	coinit();

    if (!(sp = cp->screen) && !(sp = OpenScreen()))
	return ENOCSI;

    cp->screen = sp;
    sp->user = (void *)cp;
    cp->scrflags = sp->flags;

    if (!sp->depth)
    {
	sp->depth = console_depth;
	sp->width = 640;
	if (console_modes & SM_NONLACE)
	{
	    sp->height = (PAL?256:200);
	    sp->modes = Sm_HIRES;
	}
	else
	{
	    sp->height = (PAL?512:400);
	    sp->modes = Sm_HIRES|Sm_ILACE;
	}
    }
    cp->bitmap.depth = sp->depth;
    cp->bitmap.height = sp->height;
    cp->bitmap.width = sp->width;

    if (!(cp->copbuf[0] =
	  (unsigned short *)AllocMem(2*COPSIZE(sp), MEMF_CHIP)))
	return ENOMEM;
    cp->copbuf[1] = cp->copbuf[0] + COPSIZE(sp)/sizeof (unsigned short);

    for ( i=0 ; i<cp->bitmap.depth ; ++i )
    {
	if (!(cp->bitmap.bpl[i] =
	      (unsigned char *)AllocMem(PLANESIZE(cp), MEMF_CHIP)))
	{
	    printf("console bitmap allocation failed at plane %d of %d\n",
		   i, cp->bitmap.depth);
	    while (i>0)
		FreeMem(cp->bitmap.bpl[--i], PLANESIZE(cp));
	    FreeMem((void *)cp->copbuf[0], 2*COPSIZE(sp));
	    cp->copbuf[0] = (unsigned short *)0;
	    return ENOMEM;
	}
	bzero((caddr_t)cp->bitmap.bpl[i], PLANESIZE(cp));
    }

    {
	static const struct scolor black = { 0x00, 0x00, 0x00, 0x00 };
	static const struct scolor white = { 0xFF, 0xFF, 0xFF, 0xFF };

	sp->ucolor[0] = black;
	sp->ucolor[1] = white;
	sp->ucolor[NUCOLREGS(sp)/2] = white;
	convert_ucolors(sp);
    }

    sp->row = sp->col = 0;
    sp->bmap = &cp->bitmap;

    cp->bitmap.offset = 0;
    sp->fgpen = 1;
    sp->bgpen = 0;
    sp->drawmode = JAM2;
    SetFont(sp, 0, (struct font *)0);	/* Select default font */
    SetFont(sp, 1, (struct font *)0);	/* Select default font */
    SetFont(sp, 2, (struct font *)0);	/* Select default font */
    SetFont(sp, 3, (struct font *)0);	/* Select default font */
    cp->c_state = cp->c_attr = 0;
    cp->c_argc = cp->c_curarg = 0;
    revchar(cp);

    if (sp->modes & Sm_HEDLEY)
    {
	sp->flags |= Sf_VBCALL;		/* Arrange to be called every VB */
	sp->modes &= ~Sm_ILACE;
    }

    cp->flags |= CF_INIT;
    sp->mifunc = consmisc;
    sp->kbfunc = conskb;

    return 0;
}


static void consclose(cp)
register struct console *cp;
{
    register int i;
    if (cp->screen)
    {
	int copsize = 2*COPSIZE(cp->screen);
	CloseScreen(cp->screen);
	if (cp->copbuf[0])
	    FreeMem((void *)cp->copbuf[0], copsize);
    }

    for ( i=0 ; i<cp->bitmap.depth ; ++i )
	if (cp->bitmap.bpl[i])
	    FreeMem(cp->bitmap.bpl[i], PLANESIZE(cp));
    bzero((caddr_t)cp, sizeof (*cp));
}


/*
 * Open a console stream.
 */
static int coopen(rq, devp, flag, sflag, credp)
struct queue *rq;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    int oldpri;
    register struct console *cp;
    int mindev;

    if (sflag)
	return EINVAL;

    mindev = getminor(*devp);
    if (mindev >= MAXCONSOLES)
	return ENXIO;

    cp = &consoles[mindev];

    oldpri = splscr();
    if (!(cp->flags & CF_OPEN))
    {
	if (!cp->screen && !(cp->screen = OpenScreen()))
	{
	    splx(oldpri);
	    return EAGAIN;
	}
	cp->flags |= CF_OPEN;
    }

    WR(rq)->q_ptr = rq->q_ptr = (caddr_t)cp;

    {
	register struct stroptions *sop;
	mblk_t *mop = allocb(sizeof (struct stroptions), BPRI_MED);
	if (!mop)
	{
	    splx(oldpri);
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

    if (!cp->tty ||
	!(cp->tty->t_state & (ISOPEN|WOPEN)))
    {
	cp->tty = &co_tty[mindev];
	cp->tty->t_rdqp = rq;
	cp->tty->t_dev = mindev;
	cp->tty->t_iflag = ICRNL|IXON|BRKINT|IGNPAR;
	cp->tty->t_oflag = OPOST|ONLCR;
	cp->tty->t_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK;
	cp->tty->t_cflag = CREAD|CLOCAL|CS8|B38400;
	if (mindev==0)
	    cp->tty->t_cflag &= ~HUPCL;
    }

    cp->tty->t_state &= ~WOPEN;
    cp->tty->t_state |= ISOPEN;
    cp->tty->t_state |= CARR_ON;

    splx(oldpri);
    return 0;
}


/*
 * Close a console stream.
 */
static int coclose(q, oflag, credp)
struct queue *q;
int oflag;
cred_t *credp;
{
    int oldpri = splscr();
    register struct console *cp = (struct console *)q->q_ptr;
    register struct strtty *tp = cp->tty;

    if (tp && (tp->t_state & ISOPEN))
    {
	int cflag = tp->t_cflag;

	if (!(oflag & (FNDELAY|FNONBLOCK)))
	{
	    while ((tp->t_state & CARR_ON) &&
		   (WR(q)->q_first ||
		    tp->t_state & BUSY ||
		    tp->t_out.bu_bp))
	    {
		if (!getoblk(tp))
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

	if (tp->t_out.bu_bp)
	{
	    freeb(tp->t_out.bu_bp);
	    tp->t_out.bu_bp = NULL;
	}
	tp->t_state &= ~(ISOPEN|BUSY);
	tp->t_state &= ~CARR_ON;
	tp->t_rdqp = NULL;

	cp->tty = (struct strtty *)0;
	if (cflag&HUPCL)
	    consclose(cp);
    }

    splx(oldpri);
    return 0;
}


static unsigned int blklen(bp)
register mblk_t *bp;
{
    return bp->b_wptr - bp->b_rptr;
}


/*
 * Write put procedure.
 */
static int wput(q, bp)
struct queue *q;
mblk_t *bp;
{
    register struct console *cp = (struct console *)q->q_ptr;
    register struct strtty *tp = cp->tty;
    int s;
    mblk_t *bp1;

    /*printf("wput(0x%x, 0x%x), cp=0x%x, tp=0x%x\n", q, bp, cp, tp);*/
    /*printf("wp: %x, %x, %x\n", bp->b_datap->db_type, blklen(bp), *(ulong *)bp->b_rptr);*/

    switch (bp->b_datap->db_type)
    {
    case M_DATA:
/*	printf("cowput: got a DATA message, 0x%x bytes (%x %x %x)\n",
	       blklen(bp), bp->b_rptr[0], bp->b_rptr[1], bp->b_rptr[2]);*/
	if (!(tp->t_state & CARR_ON))
	{
	    putq(q, bp);
	    return 0;
	}
	s = splscr();
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
	putioc(q, bp);
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
	s = splscr();
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
	/* Can't BREAK the screen */
	freemsg(bp);
	break;

    case M_START:
	s = splscr();
	coproc(tp, T_RESUME);
	splx(s);
	freemsg(bp);
	getoblk(tp);
	break;

    case M_STOP:
	s = splscr();
	coproc(tp, T_SUSPEND);
	splx(s);
	freemsg(bp);
	break;

    case M_DELAY:
	s = splscr();
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
	/* Read queue is always up-to-date */
	freemsg(bp);
	break;

    default:
	printf("cowput: got an unknown message 0x%x\n",
	       bp->b_datap->db_type);
	freemsg(bp);
	break;
    }
}


/*
 * Ioctl write put routine.
 */
static void putioc(q, bp)
queue_t *q;
mblk_t *bp;
{
    register struct console *cp = (struct console *)q->q_ptr;
    struct strtty *tp = cp->tty;
    struct iocblk *iocbp = (struct iocblk *)bp->b_rptr;

    mblk_t *bp1;

    switch (iocbp->ioc_cmd)
    {
    case TCSETA:
    case TCSETS:
	if (tp->t_state & BUSY)
	    putbq(q, bp);		/* queue these for later */
	else
	    srvioc(q, bp);
	return;

    case TCGETA:
    case TCGETS:			/* immediate parm retrieve */
	srvioc(q, bp);
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
	srvioc(q, bp);
}


/* check_scrtype returns zero if scrtype is legal, otherwise it */
/* tries to modify it so that it is legal but still within the user's */
/* specifications, and returns zero if this succedes.  Nonzero return */
/* indicates scrtype is not legal and should be rejected. */
int check_scrtype(scrtype)
struct scrtype *scrtype;
{
    switch (scrtype->type)
    {
    case ST_ANY:
	scrtype->type = ST_AMIGA;
	/* Fall through */
    case ST_AMIGA:
	break;
    default:
	dprintf(("check_scrtype: invalid type 0x%x\n", scrtype->type));
	return 1;
    }

    if (scrtype->modes & (SM_HAM|SM_HALFBRITE))
	scrtype->modes |= SM_LORES;

    if (scrtype->modes & SM_CHUNKY)
    {
	/* Chunky mode currently only supports 16-bit color. */
	if (scrtype->dispz != 16)
	    return 1;
    }

    /* Reject requests that just don't make sense. */
    if ((scrtype->modes&SM_INTERLACE && scrtype->modes&SM_NONLACE) ||
	(scrtype->modes&SM_HEDLEY && scrtype->modes&SM_NONHEDLEY) ||
	(scrtype->modes&SM_INTERLACE && scrtype->modes&SM_HEDLEY) ||
	(scrtype->modes&SM_HIRES && scrtype->modes&SM_LORES) ||
	(scrtype->modes&SM_OVERSCAN && scrtype->modes&SM_NONOVERSCAN) ||
	(scrtype->flags & ~(SF_LESSPLANES|SF_MOREPLANES|
			    SF_LESSRES|SF_MORERES)) ||
	(scrtype->modes & ~(SM_HEDLEY|SM_NONHEDLEY|
			    SM_INTERLACE|SM_NONLACE|
			    SM_HIRES|SM_LORES|
			    SM_OVERSCAN|SM_NONOVERSCAN|
			    SM_HAM|SM_HALFBRITE|SM_CHUNKY)))
    {
	dprintf(("check_scrtype: nonsense modes 0x%x or flags 0x%x\n",
		 scrtype->modes, scrtype->flags));
	return 1;
    }

    /* If no HIRES/LORES/LACE/NONLACE/HEDLEY/NONHEDLEY constraints, */
    /* decide which to use. */
    if (!(scrtype->modes & (SM_HIRES|SM_LORES|SM_CHUNKY)))
    {
	if (scrtype->dispx > LORES_SCRWIDTH+displayadjust.xoverscan)
	    if (scrtype->dispx >= HIRES_SCRWIDTH ||
		(scrtype->flags&SF_MORERES))
		scrtype->modes |= SM_HIRES;
	else
		scrtype->modes |= SM_LORES;
    }

    /* Only multiples of 16 are allowed for the width. */
    {
	int mask = (scrtype->modes & (SM_HIRES|SM_CHUNKY)) ? 31 : 15;
	if (scrtype->dispx & mask)
	    if (scrtype->flags&SF_MORERES)
		scrtype->dispx = (scrtype->dispx+mask) & ~mask;
	    else if (scrtype->flags&SF_LESSRES)
		scrtype->dispx &= ~mask;
	    else
	    {
		dprintf(("check_scrtype: invalid width 0x%x\n", scrtype->dispx));
		return 1;
	    }
    }

    /* Consider using hedley mode. */
    if (!(scrtype->modes&(SM_INTERLACE|SM_LORES|SM_HEDLEY|SM_NONHEDLEY)) &&
	(scrtype->dispx>=HEDLEY_SCRWIDTH ||
	 (scrtype->flags&SF_MORERES &&
	  scrtype->dispx>=HIRES_SCRWIDTH+(displayadjust.xoverscan*2))))
    {
	/* Note: if user asks for lots of res and planes, and is willing */
	/* to have less res but not fewer bitplanes, don't use hedley mode. */
	if (!(scrtype->flags&SF_LESSRES) ||
	    scrtype->dispz<=2 ||
	    scrtype->flags&SF_LESSPLANES)
	    scrtype->modes |= SM_HEDLEY;
    }
    if (!(scrtype->modes & (SM_HEDLEY|SM_INTERLACE|SM_NONLACE)))
	scrtype->modes |= ((scrtype->dispy>=LACE_SCRHEIGHT ||
			    (scrtype->flags&SF_MORERES &&
			     scrtype->dispy>=NORM_SCRHEIGHT+(displayadjust.yoverscan*2))) ?
			   SM_INTERLACE : SM_NONLACE);

    /* Check width and height */
    {
	int maxwidth, minwidth, maxheight, minheight;
	if (scrtype->modes & SM_CHUNKY)
	{
	    maxwidth = 1024;
	    minwidth = 320;
	    maxheight = 768;
	    minheight = 200;
	}
	else
	{
	    maxwidth = (scrtype->modes&SM_HEDLEY ? HEDLEY_SCRWIDTH :
		(scrtype->modes&SM_HIRES ?
		 HIRES_SCRWIDTH + displayadjust.xoverscan*2 :
		 LORES_SCRWIDTH + displayadjust.xoverscan));
	    minwidth = (scrtype->modes&SM_HEDLEY ? HEDLEY_SCRWIDTH :
		scrtype->modes&SM_HIRES ? HIRES_SCRWIDTH :
		LORES_SCRWIDTH);
	    maxheight = (scrtype->modes&SM_HEDLEY ? HEDLEY_SCRHEIGHT :
		(scrtype->modes&SM_INTERLACE ?
		 LACE_SCRHEIGHT + displayadjust.yoverscan*2 :
		 NORM_SCRHEIGHT + displayadjust.yoverscan));
	    minheight = (scrtype->modes&SM_HEDLEY ? HEDLEY_SCRHEIGHT :
		scrtype->modes&SM_INTERLACE ? LACE_SCRHEIGHT :
		NORM_SCRHEIGHT);
	}
	minwidth &= 0xffff;
	maxheight &= 0xffff;
	minheight &= 0xffff;
#endif

	if (scrtype->dispx > maxwidth)
	    if (scrtype->flags&SF_LESSRES)
		scrtype->dispx = maxwidth;
	    else
	    {
		dprintf(("check_scrtype: 0x%x>maxwidth(0x%x)\n",
			 scrtype->dispx, maxwidth));
		return 1;
	    }
	if (scrtype->dispx < minwidth)
	    if (scrtype->flags&SF_MORERES)
		scrtype->dispx = minwidth;
	    else
	    {
		dprintf(("check_scrtype: 0x%x<minwidth(0x%x)\n",
			 scrtype->dispx, minwidth));
		return 1;
	    }
	if (scrtype->dispy > maxheight)
	    if (scrtype->flags&SF_LESSRES)
		scrtype->dispy = maxheight;
	    else
	    {
		dprintf(("check_scrtype: 0x%x>maxheight(0x%x)\n",
			 scrtype->dispy, maxheight));
		return 1;
	    }
	if (scrtype->dispy < minheight)
	    if (scrtype->flags&SF_MORERES)
		scrtype->dispy = minheight;
	    else
	    {
		dprintf(("check_scrtype: 0x%x<minheight(0x%x)\n",
			 scrtype->dispy, minheight));
		return 1;
	    }
    }

    /* Check # bitplanes */
    {
	if (scrtype->modes & SM_CHUNKY)
	{
	    if (scrtype->dispz != 16)
		return 1;
	}
	else
	{
	    int maxplanes = (scrtype->modes & SM_HEDLEY) ? 2 :
		(scrtype->modes & SM_HIRES) ? 4 : 5;
	    if ((scrtype->modes & (SM_HAM|SM_HALFBRITE)) &&
		scrtype->modes & SM_LORES)
		maxplanes = 6;
	    if (scrtype->dispz > maxplanes)
		if (scrtype->flags & SF_LESSPLANES)
		    scrtype->dispz = maxplanes;
		else
		{
		    dprintf(("check_scrtype: 0x%x>maxplanes(0x%x)\n",
			     scrtype->dispz, maxplanes));
		    return 1;
		}
	}
    }

    return 0;
}


#define RETURN(type, rval, error) \
	do { \
	    mp->b_datap->db_type = type; \
	    freemsg(unlinkb(mp)); \
	    iocbp->ioc_count = 0; \
	    iocbp->ioc_rval = (rval); \
	    iocbp->ioc_error = (error); \
	    putnext(RD(q), mp); \
	    return; \
	} while (0)

#define NEEDCOPYIN(size, arg, private) \
	do { \
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr; \
	    mp->b_datap->db_type = M_COPYIN; \
	    creq->cq_addr = (arg); \
	    mp->b_wptr = mp->b_rptr + sizeof *creq; \
	    creq->cq_size = (size); \
	    creq->cq_flag = 0; \
	    creq->cq_private = (mblk_t *)(private); \
	    putnext(RD(q), mp); \
	    return; \
	} while (0)

#define NEEDCOPYOUT(size, arg, private) \
	do { \
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr; \
	    mp->b_datap->db_type = M_COPYOUT; \
	    creq->cq_addr = (arg); \
	    mp->b_wptr = mp->b_rptr + sizeof *creq; \
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + (size); \
	    creq->cq_size = (size); \
	    creq->cq_flag = 0; \
	    creq->cq_private = (mblk_t *)(private); \
	    putnext(RD(q), mp); \
	    return; \
	} while (0)

static void
srvioc(q, mp)
queue_t *q;
mblk_t *mp;
{
    register struct console *cp = (struct console *)q->q_ptr;
    register struct screen *sp = cp->screen;
    struct strtty *tp = cp->tty;
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
	    RETURN(M_IOCACK, 0, 0);
    }

/*
    printf("cosrvioc: %s message, command 0x%x\n",
	   (mp->b_datap->db_type == M_IOCDATA ? "IOCDATA" :
	    mp->b_datap->db_type == M_IOCTL ? "IOCTL" : "??"),
	    iocbp->ioc_cmd);
*/

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

	    coparam(cp);
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

	    coparam(cp);
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

	    coparam(cp);
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

	    coparam(cp);
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

	    cb->c_iflag = (ushort)tp->t_iflag;
	    cb->c_cflag = (ushort)tp->t_cflag;

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

	    if (!(bp1 = allocb(sizeof (struct termios), BPRI_MED)))
	    {
		putbq(q, mp);
		bufcall(sizeof (struct termios), BPRI_MED, getoblk, (long)tp);
		return;
	    }
	    mp->b_cont = bp1;
	    cb = (struct termios *)mp->b_cont->b_rptr;

	    cb->c_iflag = tp->t_iflag;
	    cb->c_cflag = tp->t_cflag;

	    mp->b_cont->b_wptr += sizeof (struct termios);
	    mp->b_datap->db_type = M_IOCACK;
	    iocbp->ioc_count = sizeof (struct termios);
	    putnext(RD(q), mp);
	    break;
	}

    case TCSBRK:
	RETURN((mp->b_cont ? M_IOCACK : M_IOCNAK), 0, 0);

    case TIOCSTI:
	/* Simulate typing of a character at the terminal. */
	{
	    register mblk_t *bp;

	    if (bp = allocb(1, BPRI_MED))
	    {
		if (canput(RD(q)->q_next))
		{
		    *bp->b_wptr++ = *mp->b_cont->b_rptr;
		    putnext(RD(q), bp);
		}
		else
		    freemsg(bp);
	    }

	    mp->b_datap->db_type = M_IOCACK;
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);
	    break;
	}

    case 'Beep':			/* Untested & Unsupported */
	{
	    struct bwave *p;
	    caddr_t arg;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		arg = *(caddr_t *)mp->b_cont->b_rptr;
		if (arg)
		    NEEDCOPYIN(sizeof (struct bwave), arg, (mblk_t *)arg);
		else
		    p = (struct bwave *)0;
	    }
	    else
	    {
		struct bwave *ubwp;

		(void)pullupmsg(mp->b_cont, -1);
		ubwp = (struct bwave *)mp->b_cont->b_rptr;

		if (mp->b_datap->db_type == M_IOCDATA &&
		    blklen(mp->b_cont) == sizeof (struct bwave))
		{
		    if ((ubwp->flags != 0) ||
			(ubwp->nbytes < sizeof *ubwp) ||
			(ubwp->nbytes > 0x10000))
			RETURN(M_IOCNAK, 0, ENOEXEC);
		    arg = (caddr_t)((struct copyresp *)mp->b_rptr)->cp_private;
		    NEEDCOPYIN(ubwp->nbytes, arg, (mblk_t *)1);
		}
		else if (blklen(mp->b_cont) == ubwp->nbytes)
		{
		    p = (struct bwave *)AllocMem(ubwp->nbytes, MEMF_CHIP);
		    if (!p)
			RETURN(M_IOCNAK, 0, ENOMEM);
		    bcopy((caddr_t)ubwp, (caddr_t)p, ubwp->nbytes);
		}
		else
		    RETURN(M_IOCNAK, 0, EINVAL);
	    }
	    {
		extern struct bwave *bwave;
		if (bwave)
		    FreeMem(bwave, bwave->nbytes);
		bwave = p;
	    }
	    RETURN(M_IOCACK, 0, 0);
	}
	break;

    case SIOC:
	RETURN(M_IOCACK, 0, 0);

    case SIOCDISPLAYTYPE:
	RETURN(M_IOCACK, displaytype, 0);

    case SIOCSETDISPLAYTYPE:
	{
	    unsigned int arg = *(unsigned int *)mp->b_cont->b_rptr;
	    if ((displaytype & ~DT_CHANGEABLE) != (arg & ~DT_CHANGEABLE))
		RETURN(M_IOCNAK, 0, EINVAL);
	    displaytype = (displaytype & ~DT_CHANGEABLE) | (arg & DT_CHANGEABLE);
	    if (displaytype & DT_HEDLEY)
		displaytype |= DT_MONOCHROME;
	}
	RETURN(M_IOCACK, 0, 0);

    case SIOCSETTYPE:
	{
	    struct scrtype *scrtype;
	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYIN(sizeof *scrtype, *(caddr_t *)mp->b_cont->b_rptr,
			   (mblk_t *)1);
	    if (blklen(mp->b_cont) < sizeof *scrtype)
		RETURN(M_IOCNAK, 0, 0);
	    scrtype = (struct scrtype *)mp->b_cont->b_rptr;

	    if (cp->flags&CF_INIT)
		RETURN(M_IOCNAK, 0, EEXIST);
	    if (!(displaytype&DT_HEDLEY))
		scrtype->modes |= SM_NONHEDLEY;
	    if (check_scrtype(scrtype))
		RETURN(M_IOCNAK, 0, EINVAL);

	    sp->width = scrtype->dispx;
	    sp->height = scrtype->dispy;
	    sp->depth = scrtype->dispz;
	    sp->modes |=
		((scrtype->modes&SM_HAM ? Sm_HAM : 0) |
		 (scrtype->modes&SM_HALFBRITE ? Sm_HALFBRITE : 0) |
		 (scrtype->modes&SM_INTERLACE ? Sm_ILACE : 0) |
		 (scrtype->modes&SM_HIRES ? Sm_HIRES : 0) |
		 (scrtype->modes&SM_HEDLEY ? Sm_HEDLEY_F4 : 0));
	    RETURN(M_IOCACK, 0, 0);
	}

    case SIOCGETTYPE:
	{
	    struct scrtype *scrtype;
	    caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;
	    if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
		RETURN(M_IOCNAK, 0, error);

	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(sizeof *scrtype, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);

	    scrtype = (struct scrtype *)mp->b_cont->b_wptr;
	    mp->b_cont->b_wptr += sizeof *scrtype;

	    scrtype->flags = 0;
	    scrtype->type = ST_AMIGA;
	    scrtype->dispx = sp->width;
	    scrtype->dispy = sp->height;
	    scrtype->dispz = sp->depth;
	    scrtype->modes =
		((sp->modes&Sm_HAM ? SM_HAM : 0) |
		 (sp->modes&Sm_HALFBRITE ? SM_HALFBRITE : 0) |
		 (sp->modes&Sm_ILACE ? SM_INTERLACE : 0) |
		 (sp->modes&Sm_HIRES ? SM_HIRES : 0) |
		 (sp->modes&Sm_HEDLEY ? SM_HEDLEY : 0));
	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYOUT(sizeof *scrtype, arg, NULL);
	    else
		RETURN(M_IOCACK, 0, 0);
	}
	break;

    case TIOCGWINSZ:
	if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
	    RETURN(M_IOCNAK, 0, error);
	{
	    struct winsize *size;
	    caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;
	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(sizeof *size, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);
	    size = (struct winsize *)mp->b_cont->b_rptr;
	    size->ws_row = sp->bmap->height/sp->font[0]->f_height;
	    size->ws_col = sp->bmap->width/sp->font[0]->f_width;
	    size->ws_ypixel = sp->bmap->height;
	    size->ws_xpixel = sp->bmap->width;
	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYOUT(sizeof *size, arg, NULL);
	    else
		RETURN(M_IOCACK, 0, 0);
	}
	break;

    case SIOCWINSIZE:
	if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
	    RETURN(M_IOCNAK, 0, error);
	{
	    struct swinsize *size;
	    caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;
	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(sizeof *size, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);
	    size = (struct swinsize *)mp->b_cont->b_rptr;
	    size->charsy = sp->bmap->height/sp->font[0]->f_height;
	    size->charsx = sp->bmap->width/sp->font[0]->f_width;
	    size->pixsy = sp->bmap->height;
	    size->pixsx = sp->bmap->width;
	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYOUT(sizeof *size, arg, NULL);
	    else
		RETURN(M_IOCACK, 0, 0);
	}
	break;

    case SIOCFRONT:
	if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
	    RETURN(M_IOCNAK, 0, error);
	DisplayScreen(sp);
	RETURN(M_IOCACK, 0, 0);
	break;

    case SIOCBACK:
	HideScreen(sp);
	RETURN(M_IOCACK, 0, 0);
	break;

    case SIOCACTIVATE:
	if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
	    RETURN(M_IOCNAK, 0, error);
	SelectScreen(sp);
	RETURN(M_IOCACK, 0, 0);
	break;

    case SIOCGROUPFRONT:
	if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
	    RETURN(M_IOCNAK, 0, error);
	GroupFront(sp);
	RETURN(M_IOCACK, 0, 0);
	break;

    case SIOCGETKMAP:
	{
	    struct keymap *p;
	    struct keymap *ukmp;
	    caddr_t arg;
	    unsigned int maxlen;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		arg = *(caddr_t *)mp->b_cont->b_rptr;
		NEEDCOPYIN(sizeof *ukmp, arg, (mblk_t *)arg);
	    }

	    (void)pullupmsg(mp->b_cont, -1);

	    if (blklen(mp->b_cont) != sizeof *ukmp)
		RETURN(M_IOCNAK, 0, EINVAL);

	    maxlen = ((struct keymap *)mp->b_cont->b_rptr)->km_length;

	    p = GetKmap(sp);
	    maxlen = min(maxlen, p->km_length);

	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(maxlen, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);

	    bcopy((caddr_t)p, (caddr_t)mp->b_cont->b_wptr, maxlen);
	    mp->b_cont->b_wptr += maxlen;

	    arg = (caddr_t)((struct copyresp *)mp->b_rptr)->cp_private;
	    NEEDCOPYOUT(maxlen, arg, NULL);
	}
	break;

    /* Only superuser can set system-wide default kmap. */
    case SIOCSETDEFKMAP:
	if (!suser(iocbp->ioc_cr))
	    RETURN(M_IOCNAK, 0, EPERM);
	/* Fall through */
    case SIOCSETKMAP:
#ifdef SOMEDAY
	/* User must have r/w permissions to change kmap. */
	if ((mode&(FREAD|FWRITE)) != (FREAD|FWRITE))
	    RETURN(M_IOCNAK, 0, EPERM);
#endif
	{
	    unsigned char *p;
	    caddr_t arg;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		arg = *(caddr_t *)mp->b_cont->b_rptr;
		if (arg)
		    NEEDCOPYIN(sizeof (struct keymap), arg, (mblk_t *)arg);
		else
		    p = (unsigned char *)0;
	    }
	    else
	    {
		struct keymap *ukmp;

		(void)pullupmsg(mp->b_cont, -1);
		ukmp = (struct keymap *)mp->b_cont->b_rptr;

		if (mp->b_datap->db_type == M_IOCDATA &&
		    blklen(mp->b_cont) == sizeof (struct keymap))
		{
		    if ((ukmp->km_magic != KM_MAGIC) ||
			(ukmp->km_length < sizeof *ukmp) ||
			(ukmp->km_length > 0x10000) ||
			(ukmp->km_count != 0))
			RETURN(M_IOCNAK, 0, ENOEXEC);
		    arg = (caddr_t)((struct copyresp *)mp->b_rptr)->cp_private;
		    NEEDCOPYIN(ukmp->km_length, arg, (mblk_t *)1);
		}
		else if (blklen(mp->b_cont) == ukmp->km_length)
		{
		    p = (unsigned char *)AllocMem(ukmp->km_length, 0);
		    if (!p)
			RETURN(M_IOCNAK, 0, ENOMEM);
		    bcopy((caddr_t)ukmp, (caddr_t)p, ukmp->km_length);
		}
		else
		    RETURN(M_IOCNAK, 0, EINVAL);
	    }
	    if (iocbp->ioc_cmd == SIOCSETKMAP)
		SetKmap(sp, (struct keymap *)p);
	    else
		SetDefKmap((struct keymap *)p);
	    RETURN(M_IOCACK, 0, 0);
	}
	break;

    case SIOCGETFONT+0:
    case SIOCGETFONT+1:
    case SIOCGETFONT+2:
    case SIOCGETFONT+3:
	{
	    struct font *p;
	    struct font *uftp;
	    caddr_t arg;
	    unsigned int maxlen;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		arg = *(caddr_t *)mp->b_cont->b_rptr;
		NEEDCOPYIN(sizeof *uftp, arg, (mblk_t *)arg);
	    }

	    (void)pullupmsg(mp->b_cont, -1);

	    if (blklen(mp->b_cont) != sizeof *uftp)
		RETURN(M_IOCNAK, 0, EINVAL);

	    maxlen = ((struct font *)mp->b_cont->b_rptr)->f_length;

	    p = GetFont(sp, iocbp->ioc_cmd-SIOCGETFONT);
	    maxlen = min(maxlen, p->f_length);

	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(maxlen, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);

	    bcopy((caddr_t)p, (caddr_t)mp->b_cont->b_wptr, maxlen);
	    mp->b_cont->b_wptr += maxlen;

	    arg = (caddr_t)((struct copyresp *)mp->b_rptr)->cp_private;
	    NEEDCOPYOUT(maxlen, arg, NULL);
	}
	break;

    /* Only superuser can set system-wide default font. */
    case SIOCSETDEFFONT+0:
    case SIOCSETDEFFONT+1:
    case SIOCSETDEFFONT+2:
    case SIOCSETDEFFONT+3:
	if (!suser(iocbp->ioc_cr))
	    RETURN(M_IOCNAK, 0, EPERM);
	/* Fall through */
    case SIOCSETFONT+0:
    case SIOCSETFONT+1:
    case SIOCSETFONT+2:
    case SIOCSETFONT+3:
#ifdef SOMEDAY
	/* User must have r/w permissions to change font. */
	if ((mode&(FREAD|FWRITE)) != (FREAD|FWRITE))
	    RETURN(M_IOCNAK, 0, EPERM);
#endif
	{
	    struct font *p;
	    caddr_t arg;

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
	    {
		arg = *(caddr_t *)mp->b_cont->b_rptr;
		if (arg)
		    NEEDCOPYIN(sizeof (struct font), arg, (mblk_t *)arg);
		else
		    p = (struct font *)0;
	    }
	    else
	    {
		struct font *uftp;

		(void)pullupmsg(mp->b_cont, -1);
		uftp = (struct font *)mp->b_cont->b_rptr;

		if (mp->b_datap->db_type == M_IOCDATA &&
		    blklen(mp->b_cont) == sizeof (struct font))
		{
		    if ((uftp->f_magic != F_MAGIC) ||
			(uftp->f_flags) ||
			(uftp->f_length <= sizeof *uftp) ||
			(uftp->f_length > 0x10000) ||
			(uftp->f_count != 0) ||
			(uftp->f_height == 0) ||
			(uftp->f_width == 0))
			RETURN(M_IOCNAK, 0, ENOEXEC);
		    arg = (caddr_t)((struct copyresp *)mp->b_rptr)->cp_private;
		    NEEDCOPYIN(uftp->f_length, arg, (mblk_t *)1);
		}
		else if (blklen(mp->b_cont) == uftp->f_length)
		{
		    p = (struct font *)AllocMem(uftp->f_length, 0);
		    if (!p)
			RETURN(M_IOCNAK, 0, ENOMEM);
		    bcopy((caddr_t)uftp, (caddr_t)p, uftp->f_length);
		}
		else
		    RETURN(M_IOCNAK, 0, EINVAL);
	    }
	    if (iocbp->ioc_cmd < SIOCSETDEFFONT)
	    {
		int s = splscr();
		revchar(cp);
		SetFont(sp, iocbp->ioc_cmd-SIOCSETFONT, p);
		revchar(cp);
		splx(s);
	    }
	    else
		SetDefFont(iocbp->ioc_cmd-SIOCSETDEFFONT, p);
	    RETURN(M_IOCACK, 0, 0);
	}
	break;

    case SIOCGETNCOLREGS:
	RETURN(M_IOCACK, NUCOLREGS(sp), 0);
	break;

    case SIOCSETCMAP:
	{
	    int n=NUCOLREGS(sp);

	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYIN(n * sizeof (struct scolor),
			   *(caddr_t *)mp->b_cont->b_rptr, (mblk_t *)1);

	    (void)pullupmsg(mp->b_cont, -1);
	    if (blklen(mp->b_cont) != (n * sizeof (struct scolor)))
		RETURN(M_IOCNAK, 0, EINVAL);
	    bcopy((caddr_t)mp->b_cont->b_rptr, (caddr_t)sp->ucolor,
		  (n * sizeof (struct scolor)));
	    convert_ucolors(sp);
	    sp->flags |= Sf_NEEDCOP;
	    RETURN(M_IOCACK, 0, 0);
	}
	break;

    case SIOCGETCMAP:
	{
	    caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;
	    int n=NUCOLREGS(sp);

	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(n * sizeof (struct scolor), BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);
	    bcopy((caddr_t)sp->ucolor, (caddr_t)mp->b_cont->b_rptr,
		  n * sizeof (struct scolor));
	    NEEDCOPYOUT(n * sizeof (struct scolor), arg, NULL);
	}
	break;

    case SIOCSETDISPLAYADJUST:
	{
	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYIN(sizeof displayadjust,
			   *(caddr_t *)mp->b_cont->b_rptr, (mblk_t *)1);
	    (void)pullupmsg(mp->b_cont, -1);
	    if (blklen(mp->b_cont) != sizeof displayadjust)
		RETURN(M_IOCNAK, 0, EINVAL);
	    bcopy((caddr_t)mp->b_cont->b_rptr, (caddr_t)&displayadjust,
		  sizeof displayadjust);
	    RETURN(M_IOCACK, 0, 0);
	}
	break;
    case SIOCGETDISPLAYADJUST:
	{
	    caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;
	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(sizeof displayadjust, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);
	    bcopy((caddr_t)&displayadjust, (caddr_t)mp->b_cont->b_wptr,
		  sizeof displayadjust);
	    NEEDCOPYOUT(sizeof displayadjust, arg, NULL);
	}
	break;

    case SIOCSETGROUP:
	{
	    int ng = SetScreenGroup(sp, *(int *)mp->b_cont->b_rptr);
	    if (ng < 0)
		RETURN(M_IOCNAK, 0, EINVAL);
	    else
		RETURN(M_IOCACK, ng, 0);
	}
    case SIOCGETGROUP:
	RETURN(M_IOCACK, sp->group ? sp->group-screengroups : -1, 0);

    case SIOCGETGROUPINFO:
	{
	    struct groupinfo *gp;
	    struct screen *sp;
	    int i, n;
	    caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;
	    freemsg(mp->b_cont);
	    mp->b_cont = allocb(sizeof *gp, BPRI_MED);
	    if (!mp->b_cont)
		RETURN(M_IOCNAK, 0, ENOMEM);
	    gp = (struct groupinfo *)mp->b_cont->b_rptr;
	    bzero((caddr_t)gp, sizeof *gp);
	    gp->idstamp = screenstamp;
	    n = 0;
	    for ( i=0 ; i<MAXSCREENGROUPS ; ++i )
		for ( sp=screengroups[i].top ; sp ; sp=sp->next )
		{
		    if (n >= 20)
			break;
		    gp->scr[n].group = i;
		    gp->scr[n].id = sp-screens;
		    strncpy(gp->scr[n].name, sp->name, sizeof gp->scr[n].name);
		    ++n;
		}

	    NEEDCOPYOUT(sizeof *gp, arg, NULL);
	}

    case SIOCACTIVATESCREEN:
	{
	    struct screen *sp;
	    int sn = *(int *)mp->b_cont->b_rptr;
	    if (sn < MAXSCREENS	&&
		((sp=screens+sn)->flags & Sf_INUSE)	&&
		sp->kbfunc				&&
		sp->mifunc)
	    {
		DisplayScreen(sp);
		SelectScreen(sp);
		RETURN(M_IOCACK, 0, 0);
	    }
	    else
		RETURN(M_IOCNAK, 0, EINVAL);
	}

    case SIOCACTIVATEGROUP:
	{
	    struct screen *sp;
	    int gn = *(int *)mp->b_cont->b_rptr;
	    if (gn < MAXSCREENGROUPS)
		for ( sp=screengroups[gn].top ; sp ; sp=sp->next )
		    if (sp->kbfunc && sp->mifunc)
		    {
			DisplayScreen(sp);
			SelectScreen(sp);
			RETURN(M_IOCACK, 0, 0);
		    }

	    RETURN(M_IOCNAK, 0, EINVAL);
	}

    case SIOCSETNAME:
	{
	    if (mp->b_datap->db_type == M_IOCTL &&
		iocbp->ioc_count == TRANSPARENT)
		NEEDCOPYIN(SCRNAMESIZE,
			   *(caddr_t *)mp->b_cont->b_rptr, (mblk_t *)1);
	    (void)pullupmsg(mp->b_cont, -1);
	    if (blklen(mp->b_cont) != SCRNAMESIZE)
		RETURN(M_IOCNAK, 0, EINVAL);
	    bcopy((caddr_t)mp->b_cont->b_rptr, (caddr_t)sp->name,
		  SCRNAMESIZE);
	    TouchScreen(sp);
	    RETURN(M_IOCACK, 0, 0);
	}
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


/* Process an output block. */
/* Returns nonzero if there was nothing in the queue. */

static int getoblk(tp)
register struct strtty *tp;
{
    int s = splscr();
    struct queue *q = WR(tp->t_rdqp);
    mblk_t *bp;

    do
    {
	bp = getq(q);

	if (!bp)
	{
	    /* wakeup close write queue drain */
	    if (tp->t_state & TTIOW)
	    {
		tp->t_state &= ~(TTIOW);
		wakeup((caddr_t)&tp->t_oflag);
	    }
	    splx(s);
	    return 1;
	}

	switch (bp->b_datap->db_type)
	{
	case M_DATA:
	    /*printf("cogetoblk 0x%x: got a DATA message, %d bytes (%x %x %x)\n",
		   tp, blklen(bp), bp->b_rptr[0], bp->b_rptr[1], bp->b_rptr[2]);*/
	    if (tp->t_state & (TTSTOP | TIMEOUT))
	    {
		putbq(q, bp);
		splx(s);
		return 0;
	    }

	    {
		struct console *cp = (struct console *)q->q_ptr;
		unsigned char error;
		mblk_t *bp1;
		tp->t_state |= BUSY;
		if (!(cp->flags&CF_INIT) && (error=consinit(cp)))
		{
		    if (bp1=allocb(1, BPRI_HI))
		    {
			*bp1->b_wptr++ = error;
			bp1->b_datap->db_type = M_ERROR;
			putnext(RD(q), bp1);
		    }
		}
		else
		    scrcon_put(cp, bp->b_rptr, blklen(bp));
		tp->t_state &= ~BUSY;
	    }

	    freemsg(bp);
	    break;

	case M_IOCTL:
	case M_IOCDATA:
	    srvioc(q, bp);
	    break;

	case M_BREAK:
	    /* Can't BREAK the screen */
	    freemsg(bp);
	    break;

	default:
	    printf("cogetoblk 0x%x: got an unknown message, type 0x%x\n",
		   tp, bp->b_datap->db_type);
	    freemsg(bp);
	    break;
	}
    }
    while (!(tp->t_state & BUSY));

    splx(s);
    return 0;
}


static int wsrv(q)
queue_t *q;
{
    return 0;
}


static void flush(tp, cmd)
struct strtty *tp;
{
    int s = splscr();

    if (cmd&FWRITE)
    {
	flushq(WR(tp->t_rdqp), FLUSHDATA);
	tp->t_state &= ~(BUSY);
	tp->t_state &= ~(TBLOCK);
	if (tp->t_state & TTIOW)
	{
	    tp->t_state &= ~(TTIOW);
	    wakeup((caddr_t)&tp->t_oflag);
	}
    }
    if (cmd&FREAD)
    {
	flushq(tp->t_rdqp, FLUSHDATA);
	tp->t_state &= ~(BUSY);
	tp->t_state &= ~(TBLOCK);
    }

    splx(s);
    getoblk(tp);
}


static void delay(tp)
register struct strtty *tp;
{
    int s = splscr();
    tp->t_state &= ~TIMEOUT;
    splx(s);
    getoblk(tp);
}


/*
 * Set up paramters.
 */
static void coparam(cp)
struct console *cp;
{
    if ((cp->tty->t_state&TTSTOP) && !(cp->tty->t_iflag&IXON))
	coproc(cp->tty, T_RESUME);
}


/*
 * This does all the work.
 */
static void coproc(tp, cmd)
register struct strtty *tp;
unsigned cmd;
{
    register struct console *cp;
    int s;

    switch (cmd)
    {
    case T_TIME:
	s = splscr();
	tp->t_state &= ~TIMEOUT;
	splx(s);
	getoblk(tp);
	break;

    case T_RESUME:			/* resume output */
	s = splscr();
	tp->t_state &= ~TTSTOP;
	splx(s);
	getoblk(tp);
	break;

    case T_SUSPEND:			/* suspend output */
	s = splscr();
	tp->t_state |= TTSTOP;
	splx(s);
	break;

    case T_BREAK:
	s = splscr();
	getoblk(tp);
	splx(s);
	break;

    case T_BLOCK:
	/* Can't start/stop keyboard */
	break;

    case T_UNBLOCK:
	/* Can't start/stop keyboard */
	break;
    }
}


/*
 * The kernel's routine to put out a character on the
 * "system console screen". (Used by printf, panic, etc.).
 * Pointed to by conputc in kernel.c; called by putchar() in support.c.
 */
void coputc(c)
unsigned char c;
{
    extern char *panicstr;

    if (!displaytype)
	figuredisplaytype();
    if (!(consoles[0].flags & CF_INIT))
    {
	extern struct screen *displayedscreen;
	if (consinit(&consoles[0]))
	    return;
	(void)SetScreenGroup(consoles[0].screen, 1);
	if (!displayedscreen)
	{
	    DisplayScreen(consoles[0].screen);
	    SelectScreen(consoles[0].screen);
	}
    }

    if (panicstr)
    {
	DisplayScreen(consoles[0].screen);
	SelectScreen(consoles[0].screen);
    }

    scrcon_put(&consoles[0], &c, 1);
    if (c == '\n')
    {
	c='\r';
	scrcon_put(&consoles[0], &c, 1);
    }
}
