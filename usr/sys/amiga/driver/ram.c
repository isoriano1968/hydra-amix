/*
 * A driver for a RAM disk.
 * Used for /tmp when booting from a readonly floppy.
 */

#include "sys/param.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/open.h"
#include "sys/errno.h"
#include "sys/buf.h"
#include "sys/systm.h"
#include "sys/proc.h"
#include "sys/inline.h"
#include "sys/kmem.h"

#define NUNITS 3
#define	BPSEC		512		/* bytes per sector */
static unsigned int Sizes[NUNITS] = { 256, 1024, 4096, };
static caddr_t Buffers[NUNITS];

/*
 * Open a RAM disk.
 */
int ramopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
	unsigned int mindev = getminor(*devp);

	if (mindev >= NUNITS)
		return ENXIO;

	switch (type) {
	case OTYP_BLK:
	case OTYP_MNT:
	case OTYP_SWP:
		break;
	default:
		return ENODEV;
	}

	if (!Buffers[mindev] &&
	    !(Buffers[mindev] = kmem_alloc(Sizes[mindev] * BPSEC, 0)))
		return ENOMEM;

	return 0;
}


/*
 * Close.
 */
int ramclose(dev, mode, type, cr)
dev_t dev;
int mode;
int type;
struct cred *cr;
{
	unsigned int mindev = getminor(dev);

	if (Buffers[mindev])
		kmem_free(Buffers[mindev], Sizes[mindev] * BPSEC);
	Buffers[mindev] = 0;
	return 0;
}


/*
 * RAM disk Strategy routine.
 */
void ramstrategy(bp)
register struct buf *bp;
{
	unsigned int mindev = getminor(bp->b_edev);
	caddr_t buf, addr;

	buf = Buffers[mindev] + BPSEC*bp->b_blkno;
	addr = (caddr_t)paddr(bp);
	bp->b_resid = bp->b_bcount;

	while (bp->b_resid > 0) {
		if (bp->b_blkno >= Sizes[mindev]) {
			bp->b_error = ENXIO;
			bp->b_flags |= B_ERROR;
			break;
		} else
			if (bp->b_flags&B_READ)
				bcopy(buf, (caddr_t)vtop(addr, bp->b_proc),
				      BPSEC);
			else
				bcopy((caddr_t)vtop(addr, bp->b_proc), buf,
				      BPSEC);
		addr += BPSEC;
		buf += BPSEC;
		bp->b_resid -= BPSEC;
	}

	iodone(bp);
}


/*
 * Print an error message.
 */
void ramprint(dev, mp)
dev_t	dev;
char	*mp;
{
	printf("ramdevice(0x%x): %s\n", dev, mp);
}


/*
 * Return device size / 512 bytes.
 */
int ramsize(dev)
dev_t	dev;
{
	return Sizes[getminor(dev)];
}
