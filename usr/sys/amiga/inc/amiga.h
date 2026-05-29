#define AIOC	('A'<<8)

#ifdef SVR3				/* Simulate mmap(2) */
#define AIOCMAP	(AIOC|1)
struct amigamap {
	caddr_t physaddr;		/* What physical memory is desired */
	unsigned long nbytes;		/* How large an area */
	caddr_t virtaddr;		/* Preferred virtual address */
};
#define AMAPANYWHERE ((caddr_t)-1)	/* Let kernel choose virtaddr */
#endif

#define ACRBOT ((caddr_t)0)		/* Where chip ram is addressed */
#define ACRTOP ((caddr_t)0x200000)	/* Theoretical max end of chip ram */
#define AHWBOT ((caddr_t)0xa00000)	/* Where custom chips are addressed */
#define AHWTOP ((caddr_t)0xe00000)	/* End of custom chip area */
#define AHWLEN ((unsigned)(AHWTOP-AHWBOT))

