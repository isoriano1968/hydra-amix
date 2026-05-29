/*
 * A driver for an imaginary disk which will always return an error.
 * Used to let the floppy-boot kernel think it has a swap device for
 * a while until it can be set up.
 */

#include "sys/param.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/buf.h"
#include "sys/systm.h"
#include "sys/inline.h"


/*
 * Open the dummy disk.
 */
int dumopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
	return 0;
}


/*
 * Close.
 */
int dumclose(dev, mode, type, cr)
dev_t dev;
int mode;
int type;
struct cred *cr;
{
	return 0;
}


/*
 * Strategy routine for the dummy disk.
 */
void dumstrategy(bp)
register struct buf *bp;
{
	printf("UhOh, dumstrategy(%s block %d)\n",
	       (bp->b_flags&B_READ)?"read":"write", bp->b_blkno);
	bp->b_error = ENXIO;
	bp->b_flags |= B_ERROR;
	iodone(bp);
}


/*
 * Print an error message.
 */
void dumprint(dev, mp)
dev_t	dev;
char	*mp;
{
	printf("dumdevice(0x%x): %s\n", dev, mp);
}


/*
 * Return device size.
 */
int dumsize(dev)
dev_t	dev;
{
	return 0xffffff;  /*   "I'm big.  Trust me." -- dummydisk   */
}
