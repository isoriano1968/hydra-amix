/*----- Memory Requirement Types ---------------------------*/

#define MEMF_PUBLIC	(1<<0)		/* Amix:Ignored */
#define MEMF_CHIP	(1<<1)
#define MEMF_FAST	(1<<2)

#define MEMF_PAGEB	(1<<14)		/* Amix:PageBoundary */
#define MEMF_CLEAR	(1<<16)

extern void *AllocMem();
extern void FreeMem();
