#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/map.h>
#include <sys/kmem.h>
#include "memory.h"

/* # of bytes of memory to avoid using at address 0. */
#define SKIPZERO 2048


#define Clickshift 5			/* A "Click" is 32 bytes. */
#define Click (1<<Clickshift)
#define btoC(x) (((unsigned)(x)+(Click-1))>>Clickshift)
#define Ctob(x) ((x)<<Clickshift)

extern long chipmem;			/* From the initial memory test */


#ifdef AMIXWINDOWS_EATS_HOLES_IN_MEMORY
#define uchar unsigned char
#include "locore.h"
#undef uchar
#define chipheap (locore.xxxxx0+SKIPZERO)
#define heapsize (sizeof (locore.xxxxx0)-SKIPZERO)
#endif


#define NCHIPCHUNKS 200

static unsigned char initflag;
static struct map chipmap[NCHIPCHUNKS];


static void meminit()
{
    initflag = 1;

    /* Initialize the map used to allocate chip memory space. */
    mapinit(chipmap, NCHIPCHUNKS);
#ifdef AMIXWINDOWS_EATS_HOLES_IN_MEMORY
    /* work around the holes eaten by the window system */
    if (!displaytype)
	figuredisplaytype();
    if (PAL)
    {
	if (chipmem > 0x80000)
	    mfree(chipmap, btoC(chipmem-0x80000-4096),
		  btoC(0x80000 + 4096));
	else
	    mfree(chipmap, btoC(heapsize-4096),
		  btoC(chipheap + 4096));
    }
    else
    {
	if (chipmem > 0x80000)
	    mfree(chipmap, btoC(chipmem-0x80000), btoC(0x80000));
	mfree(chipmap, btoC(heapsize), btoC(chipheap));
    }
#else
    mfree(chipmap, btoC(chipmem-SKIPZERO), btoC(SKIPZERO));
    *(unsigned short *)0 = 0x4AFC;	/* ILLEGAL opcode to catch jsr 0 */
#endif
}


void *AllocMem(nbytes, memtype)
unsigned nbytes;
unsigned memtype;
{
    if (!initflag)
	meminit();
    if (memtype & MEMF_CHIP)
    {
	if (memtype & MEMF_PAGEB)
	{
	    unsigned char *p0 = (unsigned char *)Ctob(malloc(chipmap, btoC(nbytes+PAGESIZE)));
	    unsigned char *p1 = (unsigned char *)((unsigned long)(p0 + PAGEOFFSET) & PAGEMASK);
	    mfree(chipmap, btoC(p1-p0), btoC(p0));
	    mfree(chipmap, btoC(PAGESIZE-(p1-p0)), btoC(p1+nbytes));
	    return (void *)p1;
	}
	else
	    return (void *)Ctob(malloc(chipmap, btoC(nbytes)));
    }
    else
	return (void *)(memtype&MEMF_CLEAR?kmem_zalloc:kmem_alloc)((size_t)nbytes, 0);
}


void FreeMem(ptr, nbytes)
void *ptr;
unsigned nbytes;
{
    if ((unsigned long)ptr < 0x200000)
	mfree(chipmap, btoC(nbytes), btoC(ptr));
    else
	kmem_free(ptr, (size_t)nbytes);
}
