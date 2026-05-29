/*
 * tiga.c : Driver for direct TIGA board access.
 *
 *	Works like /dev/mem but refers to the ULowell TIGA board.  The
 *	mmap() system call can cause the hardware to be mapped into
 *	the calling process' address space.  The driver does not
 *	restrict access based on UID; the file permissions of the
 *	special file should be used for security purposes.
 */

#include	"sys/types.h"
#include	"sys/immu.h"
#include	"sys/param.h"
#include	"sys/uio.h"
#include	"sys/sysmacros.h"
#include	"sys/inline.h"
#include	"sys/errno.h"
#include	"tiga.h"


#define MAXBOARDS 4
static struct GSP_REG *boards[MAXBOARDS];


static int rdwr(dev, qwerty, uiop)
int dev;				/* Card number */
uio_rw_t qwerty;			/* I/O direction (UIO_READ/UIO_WRITE) */
struct uio *uiop;
{
	unsigned long n, count;
	int error;

	if ((count=uiop->uio_resid) == 0)
		return 0;
	if (uiop->uio_offset < 0 ||
	    uiop->uio_offset > HWLEN)
		return ENXIO;
	n = uiop->uio_offset + count;		/* Calculate end address */
	if (n > HWLEN)
		count = (HWLEN-uiop->uio_offset);

	if (qwerty == UIO_WRITE && count == 0)
		return ENXIO;

	if (error=uiomove((char *)(boards[dev]) + uiop->uio_offset,
			  count, qwerty, uiop))
		return error;

	return 0;
}


int tiopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
	unsigned int dev = getminor(*devp);
	int dummy;

	if (dev >= MAXBOARDS)
		return ENXIO;
	if (!boards[dev] && !autocon(PRODUCT, dev, &boards[dev], &dummy))
		return ENXIO;
	return 0;
}


int ticlose(dev, mode, type, cr)
dev_t dev;
int mode;
int type;
struct cred *cr;
{
	return 0;
}


int tiread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	return rdwr(getminor(dev), UIO_READ, uiop);
}


int tiwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	return rdwr(getminor(dev), UIO_WRITE, uiop);
}


int timmap(dev, offset, maxprot)
dev_t dev;
off_t offset;
int maxprot;
{
	if (offset >= 0 && offset < (off_t)HWLEN)
		return phystopfn((char *)(boards[getminor(dev)]) + offset);
	return -1;
}


int tiioctl(dev, cmd, arg, mode, cr, rvalp)
dev_t dev;
int cmd;
int arg;
int mode;
struct cred *cr;
int *rvalp;
{

	switch (cmd) {
	default:
		return EINVAL;
	}

	return 0;
}
