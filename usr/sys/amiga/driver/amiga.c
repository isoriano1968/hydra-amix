/*
 * amiga.c : Driver for direct Amiga hardware access.
 *
 *	Works like /dev/mem but refers to the Amiga custom chip area.
 *	The mmap() system call can cause this area and/or some area of
 *	chip memory to be mapped into the calling process' address
 *	space.  Prior to R4.0, the AIOCMAP ioctl was used instead of
 *	mmap().  The driver does not restrict access based on UID; the
 *	file permissions of the special file should be used for
 *	security purposes.  Note that read permisison alone is
 *	sufficient to allow crashing the machine due to Amiga hardware
 *	silliness.
 */

#include	"sys/types.h"
#include	"sys/immu.h"
#include	"sys/param.h"
#include	"sys/uio.h"
#include	"sys/sysmacros.h"
#include	"sys/inline.h"
#include	"sys/errno.h"
#include	"amiga.h"

extern long chipmem;


static int amrdwr(dev, qwerty, uiop)
int dev;				/* Minor device */
uio_rw_t qwerty;			/* I/O direction (UIO_READ/UIO_WRITE) */
struct uio *uiop;
{
	register unsigned int n, count;

	if (dev)
		return ENXIO;

	if ((count=uiop->uio_resid) == 0)
		return 0;
	if (uiop->uio_offset < 0 ||
	    (caddr_t)uiop->uio_offset > AHWTOP ||
	    (uiop->uio_offset > chipmem && (caddr_t)uiop->uio_offset < AHWBOT))
		return ENXIO;
	n = uiop->uio_offset + count;		/* Calculate end address */
	if (uiop->uio_offset <= chipmem) {
		if (n > chipmem)
			count = chipmem-uiop->uio_offset;
	} else if ((caddr_t)n > AHWTOP)
		count = (off_t)(AHWTOP-uiop->uio_offset);

	if (qwerty == UIO_WRITE && count == 0)
		return ENXIO;

	while (count) {
		int error;
		n = min(count, NBPC - uiop->uio_offset % NBPC);
		if (error=uiomove(uiop->uio_offset, n, qwerty, uiop))
			return error;
		uiop->uio_offset += n;
		count -= n;
	}

	return 0;
}


int amread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	return amrdwr(getminor(dev), UIO_READ, uiop);
}


int amwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	return amrdwr(getminor(dev), UIO_WRITE, uiop);
}


int ammmap(dev, offset, maxprot)
dev_t dev;
off_t offset;
int maxprot;
{
	if ((offset >= (off_t)ACRBOT && offset < (off_t)chipmem) ||
	    (offset >= (off_t)AHWBOT && offset < (off_t)AHWTOP))
		return phystopfn(offset);
	return -1;
}


int amioctl(dev, cmd, arg, mode, cr, rvalp)
dev_t dev;
int cmd;
int arg;
int mode;
struct cred *cr;
int *rvalp;
{
	if (getminor(dev))
		return ENXIO;

	switch (cmd) {
	default:
		return EINVAL;
	}

	return 0;
}
