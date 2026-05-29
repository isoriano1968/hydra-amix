/*
 * A driver for the AMIGA 2090 ST-506 hard disk.
 * N.B.:  This has NEVER been tested under SVR4.
 */
#include "amigahr.h"
#include "amigahd.h"
#include "sys/param.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/buf.h"
#include "sys/elog.h"
#include "sys/iobuf.h"
#include "sys/systm.h"
#include "sys/inline.h"

extern paddr_t vtop();
extern time_t lbolt;

static void hdstart(), hddone(), hdinsq(), hdremq();
void hdprint();


/*
 * Configurable parameters.
 */
#define	NTRY	3			/* Number of tries for I/O */
#define	MAXCNTL	1			/* Maximum number of controllers */
#define	MAXPDEV	2			/* Maximum physical devices */
#define	MAXVDEV	8			/* Maximum virtual devices */
#define	MAXTRAN	256			/* Maximum single block transfer */

#define	sphi	spl2			/* Priority */


/*
 * To access the controller number, the controller unit,
 * the physical and virtual unit number on a device.
 */
#define	cntl(d)	(((d)>>6)&0x0003)
#define	pdev(d)	(((d)>>3)&0x0007)
#define	vdev(d)	((d)&0x0007)
#define	rdev(d)	((d)&0xFFF8)

/*
 * More constants.
 */
#define	L2ALIGN	9			/* Log (base 2) of alignment */
#define	L2BLOCK	9			/* Log (base 2) of blocksize */
#define	SIBLOCK	512			/* Size of block */

/*
 * Standard UNIX kludges.
 * We also depend on b_actf and b_actl corresponding to av_forw and av_back
 * as a circular queue is maintained.  More precisely, the offsets that
 * correspond to av_forw and av_back in the buffer structure must not be
 * used.
 */
#define	acttype	paddr_t

#define	acts	io_addr
#define	qcnt	io_s2
#define	dirn	io_s1

/*
 * Move to the next entry in queue depending on the direction flag.
 */
#define	next(tp, bp)	((struct buf *)(tp->acts=(acttype)\
			(tp->dirn?bp->av_forw:bp->av_back)))


/*
 * Initial parameters for drive.
 */
struct hdpar hdpars ={
	0xF,				/* No options, 16 uS fast step */
	(4<<4) | (612/256),		/* 4 heads, 612 cylinders */
	612%256,			/* 612 cylinders */
	128/16,				/* Start precomputation at 128 */
	128/16,				/* Reduce write current at 128 */
	17				/* Number of sectors per track */
};


/*
 * Miscellaneous tables.
 */
struct iobuf	hdctab[MAXCNTL];
struct iotime	hdstat[MAXCNTL][MAXPDEV];


/*
 * Open the hard disk.
 */
int hdopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
	register struct buf	*bp;
	register struct hdcom	*b;
	register struct hdreg	*r;
	int			c;
	int			p;
	int			i;
	int			t;
	long			a;

	c = cntl(*devp);
	p = pdev(*devp);
	if (c>=MAXCNTL || p>=MAXPDEV)
		return ENXIO;

	if ((hdctab[c].b_flags&B_ONCE) == 0) {
		hdctab[c].b_flags |= B_ONCE;

		for (i=0; i<MAXCNTL; i++) {
			bp = (struct buf *)&hdctab[i];
			bp->av_forw = bp;
			bp->av_back = bp;
			((struct iobuf *)bp)->acts = (acttype)bp;
			((struct iobuf *)bp)->dirn = 1;
		}
	}

	return 0;
}


/*
 * Close.
 */
int hdclose(dev, mode, type, cr)
dev_t dev;
int mode;
int type;
struct cred *cr;
{
	return 0;
}


/*
 * Strategy routine for the hard disk.
 */
void hdstrategy(bp)
register struct buf *bp;
{
	unsigned int	c;
	unsigned int	p;
	unsigned int	n;
	unsigned int	s;
	static void	hdstart();

	c = cntl(bp->b_edev);
	p = pdev(bp->b_edev);
	n = (bp->b_bcount + SIBLOCK - 1)  >>  L2BLOCK;

	bp->b_start = lbolt;
	hdstat[c][p].io_cnt++;
	hdstat[c][p].io_bcnt += n;
	hdctab[c].qcnt++;

	bp->b_resid = bp->b_bcount;
	s = sphi();
	hdinsq(&hdctab[c], bp);
	if (hdctab[c].b_active == 0)
		jbcall(0, c, hdstart);
	splx(s);
}


/*
 * Start the next I/O.
 */
static void hdstart(cn, rp, cp)
unsigned int		cn;
register struct hdreg	*rp;
register struct hdcom	*cp;
{
	register struct iobuf	*tp;
	register struct buf	*bp;
	int			off;
	int			bno;
	int			pad;
	int			nbl;

	tp = &hdctab[cn];
	if (tp->b_active != 0) {
		jbpass(cn);
		return;
	}
	if ((bp=(struct buf *)tp->acts) == (struct buf *)tp) {
		jbpass(cn);
		return;
	}

	tp->b_active++;
	tp->io_start = lbolt;

	off = bp->b_bcount - bp->b_resid;
	bno = bp->b_blkno + (off>>9);
	pad = vtop(paddr(bp), bp->b_proc) + off;
	if ((nbl=bp->b_resid>>L2BLOCK) >= MAXTRAN)
		nbl = MAXTRAN;

	cp->c_code = (bp->b_flags&B_READ) ? CRCOM : CWCOM;
	cp->c_usec = (pdev(bp->b_edev)<<5) | ((bno>>16)&0x1F);
	cp->c_lsec = bno;
	cp->c_nblk = nbl;
	cp->c_hdma = pad>>16;
	cp->c_mdma = pad>>8;
	cp->c_ldma = pad;
	cp->c_errs = 0xFF;
	rp->r_star = BSTAR;
}


/*
 * Hard disk interrupt.
 */
void hdintr(cn, rp, cp)
int		cn;
struct hdreg	*rp;
struct hdcom	*cp;
{
	register struct iobuf	*tp;
	register struct buf	*bp;
	long			nb;
	int			ec;

	rp->r_data = BDACK;
	rp->r_star = BSACK;

	tp = &hdctab[cn];
	if (tp->b_active == 0) {
		printf("amighd: Unexpected interrupt\n");
		return;
	}

	tp->b_active = 0;
	bp = (struct buf *)tp->acts;
	if (((ec=cp->c_errs&0x7F)) == 0) {
		if ((nb=bp->b_resid) >= (MAXTRAN<<L2BLOCK))
			nb = MAXTRAN<<L2BLOCK;
		if ((bp->b_resid-=nb) == 0) {
			hdremq(tp, bp);
			hddone(bp);
		}
	} else {
		printf("amighd(%x): c=%d, u=%d, b=%d, e=%x",
			bp->b_edev, cn, pdev(bp->b_edev), (int)bp->b_blkno, ec);
		if (++tp->b_errcnt < NTRY)
			printf(" (retrying)\n");
		else {
			printf(" (unrecoverable)\n");
			bp->b_flags |= B_ERROR;
			/* Should do error logging here */
			hdremq(tp, bp);
			hddone(bp);
		}
	}
	hdstart(cn, rp, cp);
}


/*
 * Complete a I/O request.
 */
static void hddone(bp)
register struct buf *bp;
{
	hdstat[cntl(bp->b_edev)][pdev(bp->b_edev)].io_resp += lbolt-bp->b_start;
	bp->b_error = 0;
	iodone(bp);
}


/*
 * Insert a buffer into the controller queue.
 */
static void hdinsq(tp, bp)
register struct iobuf	*tp;
register struct buf	*bp;
{
	register struct buf	*xp;
	daddr_t			b;
	int			s;

	s = sphi();
	tp->qcnt++;
	tp->b_errcnt = 0;

	b = bp->b_blkno;
	xp = (struct buf *)tp;
	while ((xp=xp->av_forw)!=(struct buf *)tp && b<(daddr_t)xp->b_blkno)
		;

	bp->av_forw = xp;
	bp->av_back = xp->av_back;
	xp->av_back->av_forw = bp;
	xp->av_back = bp;
	if ((struct buf *)tp->acts == (struct buf *)tp)
		tp->acts = (acttype)bp;
	splx(s);
}


/*
 * Remove a buffer from the controller queue.
 */
static void hdremq(tp, bp)
register struct iobuf	*tp;
register struct buf	*bp;
{
	hdstat[cntl(bp->b_edev)][pdev(bp->b_edev)].io_act += lbolt-tp->io_start;
	bp->av_forw->av_back = bp->av_back;
	bp->av_back->av_forw = bp->av_forw;
	if (next(tp, bp) == (struct buf *)tp) {
		tp->dirn ^= 1;
		next(tp, bp);
	}
}


/*
 * Print an error message.
 */
void hdprint(dev, mp)
dev_t	dev;
char	*mp;
{
	printf("hd(%x): %s\n", dev, mp);
}


/*
 * Return device size.
 */
int hdsize(dev)
dev_t	dev;
{
	/*printf("hdsize(0x%x) returning 0x%x\n", dev, 0x7fffffff);*/
	return 0x7fffffff;
}


/* NOTE: I converted this driver to SVR4 style but did not attempt */
/* to make the "raw" interface entry points work -- they still need */
/* to do that dma_pageio() thing.  -=] Ford [=- */

/*
 * Raw read from the hard disk.
 */
int hdread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	if ((uiop->uio_offset&(SIBLOCK-1))!=0 || (uiop->uio_resid&(SIBLOCK-1))!=0)
		return EINVAL;
	return uiophysio(hdstrategy, (struct buf *)0, dev, UIO_READ, uiop);
}


/*
 * Raw write to the hard disk.
 */
int hdwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	if ((uiop->uio_offset&(SIBLOCK-1))!=0 || (uiop->uio_resid&(SIBLOCK-1))!=0)
		return EINVAL;
	return uiophysio(hdstrategy, (struct buf *)0, dev, UIO_WRITE, uiop);
}


static char hardwarename[] = "A2090 ST-506";

/*
 * Ioctl.
 */
hdioctl(dev, cmd, arg, mode, cr, rvalp)
dev_t dev;
int cmd;
int arg;
int mode;
struct cred *cr;
int *rvalp;
{
	switch (cmd) {
#ifdef DIOC
	case DIOC:
		return 0;
#endif
#ifdef DIOCHARDWARENAME
	case DIOCHARDWARENAME:
		if (copyout(hardwarename, arg, sizeof hardwarename))
			return EFAULT;
		return 0;
#endif
	default:
		return EINVAL;
	}
}
