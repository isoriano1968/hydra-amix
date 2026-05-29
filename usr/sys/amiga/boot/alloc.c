#pragma pack(2)
#include <exec/types.h>
#include <exec/memory.h>
#pragma pack(4)

/*
 *	Allocate some memory, and issue a complaint if the allocation
 *	fails.
 */

static char allocmsg[] = {
    "WARNING!  Failed to allocate memory.\n"
    "AllocMem(%d,%x) failed.\n"
    "%M"
};

void *alloc (unsigned long nbytes, int flags)
{
    void *newmem;
    extern APTR AllocMem ();

    if ((newmem = (void *) AllocMem (nbytes, flags)) == NULL) {
	message (reloc (allocmsg), nbytes, flags);
    }
    return (newmem);
}

