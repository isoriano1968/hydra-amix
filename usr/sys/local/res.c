/*
 * res.c : Device driver for direct access to Resolver hardware.
 */

#include "sys/types.h"
#include "sys/immu.h"
#include "sys/param.h"
#include "sys/uio.h"
#include "sys/sysmacros.h"
#include "sys/inline.h"
#include "sys/errno.h"
#include "resolver.h"

struct cred;


#define MAXBOARDS 4
static struct GSP_REG *boards[MAXBOARDS];


static int rdwr(int dev,		/* Card number */
		uio_rw_t qwerty,	/* I/O direction (UIO_READ/UIO_WRITE) */
		struct uio *uiop)
{
    unsigned long n, count;

    if ((count=uiop->uio_resid) == 0)
	return 0;
    if (uiop->uio_offset < 0 ||
	uiop->uio_offset > HWLEN)
	return ENXIO;
    n = uiop->uio_offset + count;	/* Calculate end address */
    if (n > HWLEN)
	count = (HWLEN-uiop->uio_offset);

    if (qwerty == UIO_WRITE && count == 0)
	return ENXIO;

    return uiomove((char *)(boards[dev]) + uiop->uio_offset,
		   count, qwerty, uiop);
}


int resopen(dev_t *devp, int mode, int type, struct cred *cr)
{
    unsigned int dev = getminor(*devp);
    int dummy;

    if (dev >= MAXBOARDS)
	return ENXIO;
    if (!boards[dev] && !autocon(RESOLVER_ID, dev, &boards[dev], &dummy))
	return ENXIO;

    return 0;
}


int resclose(dev_t dev, int mode, int type, struct cred *cr)
{
    return 0;
}


int resread(dev_t dev, struct uio *uiop, struct cred *cr)
{
    return rdwr(getminor(dev), UIO_READ, uiop);
}


int reswrite(dev_t dev, struct uio *uiop, struct cred *cr)
{
    return rdwr(getminor(dev), UIO_WRITE, uiop);
}


int resmmap(dev_t dev, off_t offset, int maxprot)
{
    if (offset >= 0 && offset < (off_t)HWLEN)
	return phystopfn((char *)(boards[getminor(dev)]) + offset);
    return -1;
}


int resioctl(dev_t dev, int cmd, int arg,
	     int mode, struct cred *cr, int *rvalp)
{
    switch (cmd)
    {
    default:
	return EINVAL;
    }

    return 0;
}
