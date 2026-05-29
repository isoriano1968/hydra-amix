/*
 * zz9000.c - ZZ9000 framebuffer/mmap driver for Amiga UNIX (Amix 2.1c)
 *
 * This driver exposes the card's framebuffer via mmap and simple read/write.
 * It uses Amiga autoconfig to locate the board. Update the manufacturer and
 * product IDs for the actual ZZ9000 card.
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/uio.h"
#include "sys/sysmacros.h"
#include "sys/errno.h"
#include "sys/conf.h"
#include "sys/immu.h"
#include "sys/systm.h"
#include "sys/proc.h"
#include "sys/inline.h"

#define ZZ9000_MANUFACTURER 0x0000  /* TODO: actual manufacturer ID */
#define ZZ9000_PRODUCT      0x0000  /* TODO: actual product ID */
#define ZZ9000_CARDID       ((ZZ9000_MANUFACTURER<<16) | ZZ9000_PRODUCT)

#define MAX_BOARDS 4

struct zzboard {
    char *base;
    unsigned long size;
};

static struct zzboard zzboards[MAX_BOARDS];

static int
zzprobe(dev)
    dev_t dev;
{
    int unit = getminor(dev);
    long base, size;

    if (unit >= MAX_BOARDS)
        return 0;
    if (zzboards[unit].base)
        return 1;

    if (!autocon(ZZ9000_CARDID, unit, &base, &size))
        return 0;

    zzboards[unit].base = (char *)base;
    zzboards[unit].size = (unsigned long)size;
    return 1;
}

static int
zzrw(dev, qwerty, uiop)
    dev_t dev;
    uio_rw_t qwerty;
    struct uio *uiop;
{
    unsigned long count;
    long offset;
    int error;
    char *addr;
    int unit = getminor(dev);

    if (!zzprobe(dev))
        return ENXIO;

    addr = zzboards[unit].base;
    count = uiop->uio_resid;
    offset = uiop->uio_offset;

    if (offset < 0 || (unsigned long)offset >= zzboards[unit].size)
        return ENXIO;
    if (offset + count > zzboards[unit].size)
        count = zzboards[unit].size - offset;

    if (count == 0 && qwerty == UIO_WRITE)
        return ENXIO;

    error = uiomove(addr + offset, count, qwerty, uiop);
    return error;
}

int
zzopen(devp, mode, type, cr)
    dev_t *devp;
    int mode;
    int type;
    struct cred *cr;
{
    if (!zzprobe(*devp))
        return ENXIO;
    return 0;
}

int
zzclose(dev, mode, type, cr)
    dev_t dev;
    int mode;
    int type;
    struct cred *cr;
{
    return 0;
}

int
zzread(dev, uiop, cr)
    dev_t dev;
    struct uio *uiop;
    struct cred *cr;
{
    return zzrw(dev, UIO_READ, uiop);
}

int
zzwrite(dev, uiop, cr)
    dev_t dev;
    struct uio *uiop;
    struct cred *cr;
{
    return zzrw(dev, UIO_WRITE, uiop);
}

int
zzmmap(dev, offset, maxprot)
    dev_t dev;
    off_t offset;
    int maxprot;
{
    int unit = getminor(dev);

    if (!zzprobe(dev))
        return -1;
    if (offset < 0 || (unsigned long)offset >= zzboards[unit].size)
        return -1;

    return phystopfn(zzboards[unit].base + offset);
}

int
zzioctl(dev, cmd, arg, mode, cr, rvalp)
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
}
