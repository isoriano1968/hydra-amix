/*
 * This driver operates the floppy disk device using a format which
 * is the same as AmigaDOS.
 */
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/errno.h"
#include "sys/inline.h"
#include "sys/file.h"
#include "sys/uio.h"
#include "sys/open.h"
#include "sys/kmem.h"
#include "memory.h"
#include "hstyle.h"


/*
 * Characteristics of the floppy.
 * This stuff should really come from a flopf.h.
 */
#define	TRACKS		160		/* 80 cylinders, two heads */
#define	BPSEC		512		/* bytes per sector */
#define	NSECML		9		/* sectors per track, MS-DOS low */
#define	NSECMH		18		/* sectors per track, MS-DOS high */
#define	NSECAL		11		/* sectors per track, Amiga low */

#if 0	/* activate this if Amiga high-density floppies supported */
#define	NSECAH		22		/* sectors per track, Amiga high */
#define	BPTRKMAX	(NSECAH*BPSEC)	/* maximum bytes per track */
#else	/* activate this if Amiga high-density floppies are unsupported */
#define	BPTRKMAX	(NSECMH*BPSEC)	/* maximum bytes per track */
#endif

#define	CHIPM		(32*1024)	/* sizeof max chip-memory buffer */


#define	NTYPES		4		/* number of floppy formats */
#define	NDRIVES		4		/* maximum number of drives */
#define	NODRV		(-1)		/* illegal drive number */
#define	NOTRK		(-1)		/* illegal track number */
#define	MINSL		(1*1000000/HZ)	/* sleep if atleast 1 tick */
#define	DELPRI		PZERO		/* sleep priority in fldelay() */
#define	IDLEPRI		PPIPE		/* slave idle sleep priority */
#define	MSTOPT		(1*HZ)		/* ticks between motor timeouts */
#define	MLIFE		6		/* no. of MSTOPT's before stopping */
#define	TBCLEN		10		/* size of track buffer cache */


/*
 * The following macros crack off parts of the device number:
 *	dtodrv	- get the drive number (only valid if not slave).
 *	dfmt	- TRUE iff we are formatting.
 *	dmaid	- TRUE iff we are the slave.
 * Here is the meaning of the 8 bits of minor device:
 *	bits 5-7	drive number
 *	bit  4		format
 *	bit  3		prompt on first open
 *	bit  1-2	drive type:
 *				0	amiga low density (880K)
 *				1	(NOT IMPLIMENTED) amiga high density (1760K)
 *				2	msdos low density (720K)
 *				3	msdos high density (1440K)
 *	bit  0		0
 * Note, the special minor device 0xFF is used as the maid device.
 */
#define	dmaid(dev)	(getminor(dev) == 0xFF)
#define	dfmt(dev)	((getminor(dev) & 1<<4) != 0)
#define	dprompt(dev)	((getminor(dev) & 1<<3) != 0)
#define	dtotype(dev)	(0x3 & getminor(dev)>>1)
#define	dtodrv(dev)	(0x7 & getminor(dev)>>5)


/*
 * A dtype holds all the information associated with a type of
 * floppy.
 */
typedef struct	dtype {
	bool	(*get)(),		/* function to read a track */
		(*put)();		/* function to write a track */
	uint	nsecs,			/* sectors (=blocks) per track */
		nblocks;		/* blocks per disk (nsecs*tracks) */
}	dtype;


/*
 * A track holds all the information needed for an entry in the
 * track buffer cache.
 */
typedef struct	track {
	struct track	*next;		/* next in LRU order */
	short		drv,		/* drive owning this track */
			trk;		/* track number */
	dtype		*type;		/* track format (into dtypes) */
	bool		dirty,		/* iff must be written out */
			check;		/* check track flag for puttrk() */
	char		data[BPTRKMAX];	/* actual data */
}	track;


/*
 * External functions.
 */
extern void	flstop(),		/* stop drive motor */
		fdamtbuf(),		/* set chip memory for Amiga */
		fdmstbuf();		/* set chip memory for MS-DOS */
extern bool	flok(),			/* iff drive is ok */
		flwprot(),		/* iff drive is write protected */
		fdgetaml(),		/* read Amiga low density track */
		fdputaml(),		/* write Amiga low density track */
#if 0	/* activate this if Amiga high-density floppies supported */
		fdgetamh(),		/* read Amiga high density track */
		fdputamh(),		/* write Amiga high density track */
#endif
		fdgetmsl(),		/* read MS-DOS low density track */
		fdputmsl(),		/* write MS-DOS low density track */
		fdgetmsh(),		/* read MS-DOS high density track */
		fdputmsh();		/* write MS-DOS high density track */
extern paddr_t	vtop();


/*
 * Local variables.
 */
static track	*tbc,			/* head of track buffer cache */
		*tbcbase;		/* base of allocated tbc */
static bool	servant,		/* iff slave present */
		idle,			/* iff slave sleeping */
		chkmot	= TRUE;		/* iff we should age motors */
static uchar	mlives[NDRIVES];	/* no. MTSTOPS till we stop */
static buf_t	*head,			/* head of queued requests */
		**tail	= &head;	/* where to put next request */
static dtype	dtypes[NTYPES] = {	/* info on formats */
			{		/* amiga dos, low density */
				fdgetaml,	/* get track */
				fdputaml,	/* put track */
				NSECAL,		/* sectors per track */
				NSECAL*TRACKS	/* number of tracks */
			},
			{		/* amiga dos, high density */
				NULL,		/* get track */
				NULL,		/* put track */
				0,		/* sectors per track */
				0		/* number of tracks */
			},
			{		/* MS-DOS, low density */
				fdgetmsl,	/* get track */
				fdputmsl,	/* put track */
				NSECML,		/* sectors per track */
				NSECML*TRACKS	/* number of tracks */
			},
			{		/* MS-DOS, high density */
				fdgetmsh,	/* get track */
				fdputmsh,	/* put track */
				NSECMH,		/* sectors per track */
				NSECMH*TRACKS	/* number of tracks */
			}
		};


/*
 * Local functions.
 */
static int	slave();		/* servant idle loop */
static void	rundevice();		/* process next request */
static buf_t	*getnext();		/* get next buf to process */
static bool	tbcinit();		/* initialize t. b. cache */
static track	*findtrk();		/* return track in t. b. cache */
static void	flush();		/* flush device from t. b. cache */
static track	*incache();		/* check if drv/trk in t. b. cache */
static bool	read(),			/* read a block */
		write(),		/* write a block */
		format();		/* format a floppy */
static		prod(),			/* wakeup for fldelay() */
		chkprod();		/* wakeup for stopping motor */
static int	fdphysio();		/* perform synchronous physical I/O */
void		fdstrategy();


int
fdopen(devp, mode, type, cr)
dev_t		*devp;
int		mode,
		type;
struct cred	*cr;
{
	char		*flopbuf;
	int		drv;
	static bool	first	= TRUE;

	if (first) {
		flopbuf = (char *)AllocMem(CHIPM, MEMF_CHIP);
		if (flopbuf == NULL)
			return (ENOMEM);
		first = FALSE;
		fdamtbuf(flopbuf, CHIPM);
		fdmstbuf(flopbuf, CHIPM);
		flinit();
		if (not dmaid(*devp) && dprompt(*devp)) {
			printf("\nInsert floppy disk 2 (root file system) and press RETURN.");
			getchar();
			printf("\nBooting\n");
		}
	}
	if (dmaid(*devp)) {
		if (servant)
			return ENXIO;
		return (slave());	/* only returns if killed */
	}
	drv = dtodrv(*devp);
	if (drv >= NDRIVES)
		return (ENXIO);
	if (dtypes[dtotype(*devp)].nblocks == 0)
		return (ENXIO);
	if ((tbcbase == NULL)
	and (not tbcinit()))
		return (ENOMEM);
	if (not flok(drv))
		return (ENXIO);
	mlives[drv] = MLIFE;
	if (((mode & FWRITE) != 0)
	and (flwprot(drv))) {
		mlives[drv] = 1;	/* express turn off */
		return (EROFS);
	}
	return (0);
}


int
fdclose(dev, mode, type, cr)
dev_t dev;
int mode;
int type;
struct cred *cr;
{
	uint	drv;

	if (not dmaid(dev)) {
		drv = dtodrv(dev);
		assert(drv < NDRIVES);
		if (mlives[drv] != 0)
			mlives[drv] = 1;/* express turn off */
	}
	return (0);
}


int
fdread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	if (uiop->uio_offset == fdsize(dev)*BPSEC)
		return (0);
	if (uiop->uio_offset + uiop->uio_resid > fdsize(dev)*BPSEC)
		uiop->uio_resid = fdsize(dev)*BPSEC - uiop->uio_offset;
	return fdphysio(fdstrategy, (struct buf *)0, dev, B_READ, uiop);
}


int
fdwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	if (uiop->uio_offset == fdsize(dev)*BPSEC)
		return (ENOSPC);
	if (uiop->uio_offset + uiop->uio_resid > fdsize(dev)*BPSEC)
		uiop->uio_resid = fdsize(dev)*BPSEC - uiop->uio_offset;
	return fdphysio(fdstrategy, (struct buf *)0, dev, B_WRITE, uiop);
}


void
fdprint(dev, mesg)
dev_t	dev;
char	*mesg;
{
	printf("Floppy fd%d: %s\n", dtodrv(dev), mesg);
}


int
fdsize(dev)
dev_t	dev;
{
	assert(0 <= dtotype(dev) && dtotype(dev) < cardof(dtypes));
	return (dtypes[dtotype(dev)].nblocks);
}


void
fdstrategy(bp)
register buf_t	*bp;
{
	register uint	drv;
	int		s;
	static bool	busy;

	assert(dtodrv(bp->b_edev) < NDRIVES);
	s = splhi();
	while (not servant && busy)
		sleep((caddr_t)&busy, PRIBIO);
	busy = TRUE;
	if (not servant) {		/* Maid's day off */
		splx(s);
		drv = dtodrv(bp->b_edev);
		assert(drv < NDRIVES);
		rundevice(bp);
		flush(drv);
		mlives[drv] = 0;
	} else {
		*tail = bp;
		tail = &bp->av_forw;
		*tail = NULL;
		splx(s);
		if (idle)
			wakeup((caddr_t)&idle);
	}
	busy = FALSE;
}


static int fdphysio(strat, bp, dev, rw, uiop)
void (*strat)();
struct buf *bp;
dev_t dev;
int rw;
struct uio *uiop;
{
	static unsigned char buffer[BPSEC];
	static struct buf buf;
	int s, error=0;

	if (uiop->uio_resid % BPSEC)
		return EIO;

	while (uiop->uio_resid) {
		s = spl6();
		while (buf.b_flags & B_BUSY) {
			buf.b_flags |= B_WANTED;
			(void)sleep((caddr_t)&buf, PRIBIO+1);
		}
		(void)splx(s);

		buf.b_error = 0;
		buf.b_flags = B_KERNBUF | B_BUSY | B_PHYS | rw;
		buf.b_edev = dev;
		buf.b_blkno = uiop->uio_offset / BPSEC;
		buf.b_bcount = BPSEC;
		buf.b_un.b_addr = (caddr_t)buffer;

		if (rw == B_WRITE)
			error = uiomove(buffer, BPSEC,
					UIO_WRITE, uiop);
		if (!error) {
			(*strat)(&buf);
			s = spl6();
			while ((buf.b_flags & B_DONE) == 0)
				sleep((caddr_t)&buf, PRIBIO);
			splx(s);
			
			if ((buf.b_flags & B_ERROR) || buf.b_resid)
				error = buf.b_error ?
					buf.b_error : EIO;
		}

		(void)spl6();
		if (buf.b_flags & B_WANTED)
			wakeup((caddr_t)&buf);
		(void)splx(s);
		buf.b_flags &= ~(B_BUSY|B_WANTED|B_PHYS);

		if (error)
			return error;

		if (rw == B_READ)
			error = uiomove(buffer, BPSEC,
					UIO_READ, uiop);
		if (error)
			return error;
	}
	return 0;
}


static int
slave()
{
	register uint	s,
			drv;

	servant = TRUE;
	loop {
		s = splhi();
		while (head == NULL && not chkmot) {
			idle = TRUE;
			if ((sleep((caddr_t)&idle, IDLEPRI|PCATCH))
			and (head == NULL)) {
				servant = FALSE;
				idle = FALSE;
				for (drv = 0; drv != NDRIVES; ++drv)
					flush(drv);
				splx(s);
				return (EINTR);
			}
			idle = FALSE;
		}
		splx(s);
		if (head != NULL)
			rundevice(getnext());
		if (chkmot) {
			chkmot = FALSE;
			for (drv = 0; drv != NDRIVES; ++drv)
				if ((mlives[drv] != 0)
				and (--mlives[drv] == 0))
					flush(drv);
			timeout(chkprod, (caddr_t)&idle, MSTOPT);
		}
	}
}


static void
rundevice(bp)
register buf_t	*bp;
{
	register bool		(*func)();
	register daddr_t	blk;
	register paddr_t	to;

	assert(bp != NULL);
	if (bp->b_bcount % BPSEC) {
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
	if (dfmt(bp->b_edev)) {
		if (bp->b_flags & B_READ) {
			bp->b_flags |= B_ERROR;
			iodone(bp);
			return;
		}
		func = format;
	} else if (bp->b_flags & B_READ)
		func = read;
	else
		func = write;
	blk = bp->b_blkno;
	to = vtop(paddr(bp), bp->b_proc);
	bp->b_resid = bp->b_bcount;
	while (bp->b_resid != 0) {
		if (not (*func)(bp->b_edev, to, blk, &bp->b_flags))
			break;
		++blk;
		to += BPSEC;
		bp->b_resid -= BPSEC;
	}
	iodone(bp);
}


/*
 * Getnext returns the next buf to process, removing it from the queue.
 * It does this by first looking for a buffer which corresponds to a
 * track in the track buffer cache.  If no such buf exists, it uses
 * the first one.
 */
static buf_t	*
getnext()
{
	register buf_t	*res,
			**resp;
	register uint	s;
	register int	trk;

	assert(head != NULL);
	for (resp = &head;; resp = &res->av_forw) {
		res = *resp;
		if (res == NULL) {
			resp = &head;
			res = head;
			break;
		}
		trk = res->b_blkno / dtypes[dtotype(res->b_edev)].nsecs;
		if (incache(dtodrv(res->b_edev), trk) != NULL)
			break;
	}
	s = splhi();			/* in case strategy can interrupt */
	*resp = res->av_forw;
	if (res->av_forw == NULL)
		tail = resp;
	splx(s);
	return (res);
}


/*
 * Tbcinit initializes the track buffer cache.
 * It returns TRUE unless there is not enough memory.
 * Note, we call kmem_alloc with second argument KM_NOSLEEP
 * so that multiple fdopen's don't fall over themselves.
 * If this must be changed, then proper sleep/wakeup's must
 * be installed.
 */
static bool
tbcinit()
{
	track	*tp;
	int	i;

	assert(TBCLEN > 1);
	tp = kmem_alloc(TBCLEN * sizeof(*tp), KM_NOSLEEP);
	if (tp == NULL)
		return (FALSE);
	i = 0;
	loop {
		tp[i].drv = NODRV;
		tp[i].trk = NOTRK;
		if (++i == TBCLEN)
			break;
		tp[i-1].next = &tp[i];
	}
	tp[TBCLEN-1].next = NULL;
	tbcbase = tp;
	tbc = tp;
	return (TRUE);
}


/*
 * Findtrk is the main interface to the track buffer cache.
 * It returns a pointer to the track buffer containing the requested
 * drive/track, or NULL if there is an error reading the data.
 * It also updates the LRU order in the track buffer cache.
 * Note, findtrk() may have to write out a track, and this may get
 * errors.  In this case a message will be sent to the console by lower
 * level routines, but there is really no one for findtrk to report it to,
 * so it ignores it.
 * If the track is not in the cache then the action depends on the value
 * of the `read' flag.  If it is TRUE then the track is read in from the
 * drive.  Otherwise, the buffer is just zeroed out.  In both of these
 * cases, the `check' flag is set to the `read' flag.
 */
static track	*
findtrk(dev, trk, read)
register uint	dev,
		trk;
bool		read;
{
	register track	*father,
			*probe,
			*tp;
	uint		drv;
	dtype		*dp;

	dp = &dtypes[dtotype(dev)];
	assert(dtypes <= dp && dp < endof(dtypes));
	drv = dtodrv(dev);
	assert(drv < NDRIVES);
	father = NULL;
	probe = tbc;
	loop {
		if ((probe->drv == drv)
		and (probe->trk == trk)
		and (probe->type == dp)) {
			if (father != NULL) {
				father->next = probe->next;
				probe->next = tbc;
				tbc = probe;
			}
			return (probe);
		}
		if (probe->next == NULL)
			break;
		father = probe;
		probe = probe->next;
	}
	assert(father != NULL);		/* iff TBCLEN > 1 */
	father->next = NULL;
	probe->next = tbc;
	tbc = probe;
	if ((probe->dirty)
	and (probe->drv != NODRV)
	and (probe->trk != NOTRK)) {
		assert(dtypes <= probe->type && probe->type < endof(dtypes));
		(*probe->type->put)(probe->drv, probe->trk,
			probe->data, probe->check);
		mlives[probe->drv] = MLIFE;
	}
	if (read) {
		if (not (*dp->get)(drv, trk, probe->data)) {
			mlives[drv] = 1;	/* quick removal */
			probe->drv = NODRV;
			probe->trk = NOTRK;
			return (NULL);
		}
		probe->dirty = FALSE;
	} else {
		bzero(probe->data, sizeof(probe->data));
		probe->dirty = TRUE;
	}
	probe->check = read;
	probe->drv = drv;
	probe->trk = trk;
	probe->type = dp;
	mlives[drv] = MLIFE;
	return (probe);
}


/*
 * Flush is used to remove all entries in the track buffer cache for a
 * given drive and to turn the drive motor off.  The reason that we
 * invalidate the cache is so that the disk can safely be removed
 * (and possibly swapped with another disk).
 * As in findtrk(), we may have to write a track, and this may get an
 * error, but there is not much to be done, so we ignore it.
 */
static void
flush(drv)
register uint	drv;
{
	register track	*tp;

	for (tp = tbc; tp != NULL; tp = tp->next) {
		if (tp->drv == drv) {
			if (tp->dirty) {
				assert(dtypes <= tp->type && tp->type < endof(dtypes));
				(*tp->type->put)(tp->drv, tp->trk,
					tp->data, tp->check);
			}
			tp->drv = NODRV;
			tp->trk = NOTRK;
		}
	}
	flstop(drv);
}


/*
 * Incache returns a pointer to the entry in the track buffer cache
 * containing the specified drive/track, if any.  If it is not in
 * the cache, it returns NULL.
 */
static track	*
incache(drv, trk)
register uint	drv,
		trk;
{
	register track	*tp;

	for (tp = tbc; tp != NULL; tp = tp->next)
		if (tp->drv == drv && tp->trk == trk)
			return (tp);
	return (NULL);
}


static bool
read(dev, buf, bno, flagp)
uint	dev;
paddr_t	buf;
daddr_t	bno;
int	*flagp;
{
	uint	trk,
		sec;
	track	*tk;
	dtype	*dp;

	dp = &dtypes[dtotype(dev)];
	assert(dtypes <= dp && dp < endof(dtypes));
	if ((ulong)bno >= dp->nblocks) {
		if ((ulong)bno != dp->nblocks)
			*flagp |= B_ERROR;
		return (FALSE);
	}
	trk = bno / dp->nsecs;
	sec = (uint)bno % dp->nsecs;
	tk = findtrk(dev, trk, TRUE);
	if (tk == NULL) {
		*flagp |= B_ERROR;
		return (FALSE);
	}
	bcopy(tk->data + sec*BPSEC, buf, BPSEC);
	return (TRUE);
}


static bool
write(dev, buf, bno, flagp)
uint	dev;
paddr_t	buf;
daddr_t	bno;
int	*flagp;
{
	uint	trk,
		sec;
	track	*tk;
	dtype	*dp;

	dp = &dtypes[dtotype(dev)];
	assert(dtypes <= dp && dp < endof(dtypes));
	if ((ulong)bno >= dp->nblocks) {
		*flagp |= B_ERROR;
		return (FALSE);
	}
	trk = bno / dp->nsecs;
	sec = (uint)bno % dp->nsecs;
	tk = findtrk(dev, trk, TRUE);
	if (tk == NULL) {
		*flagp |= B_ERROR;
		return (FALSE);
	}
	bcopy(buf, tk->data + sec*BPSEC, BPSEC);
	tk->dirty = TRUE;
	return (TRUE);
}


static bool
format(dev, buf, bno, flagp)
uint	dev;
paddr_t	buf;
daddr_t	bno;
int	*flagp;
{
	dtype	*dp;

	dp = &dtypes[dtotype(dev)];
	assert(dtypes <= dp && dp < endof(dtypes));
	if (((ulong)bno < dp->nblocks)
	and ((uint)bno % dp->nsecs == 0))
		findtrk(dev, (uint)(bno/dp->nsecs), FALSE);
	return (write(dev, buf, bno, flagp));
}


/*
 * Fldelay() is called from the lowest level routines to delay for
 * a specified number of microseconds.  This is accomplished either
 * by busy waiting (which is cpu, system, compiler and optimization-level
 * dependant) or by timeouts.
 * If we have a servant and if the delay is long enough, we will set
 * a timeout and go to sleep.  Otherwise, we must busy wait.
 * Note, waiting must be static so that it is mapped in when prod()
 * is called from the clock interrupt.
 */
void
fldelay(mics)
register int	mics;
{
	register int	s;
	static bool	waiting;

	if (mics >= MINSL && servant) {
		waiting = TRUE;
		timeout(prod, (caddr_t)&waiting, 1 + HZ*mics/1000000);
		s = splhi();
		while (waiting)
			sleep((caddr_t)&waiting, DELPRI);
		splx(s);
	} else
		delayus(mics);
}


static
prod(channel)
caddr_t	channel;
{
	*(bool *)channel = FALSE;
	wakeup(channel);
}


static
chkprod(channel)
caddr_t	channel;
{
	chkmot = TRUE;
	wakeup(channel);
}
