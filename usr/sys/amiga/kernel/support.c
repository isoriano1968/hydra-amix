/*
 * Miscellaneous routines.
 */
#include "sys/types.h"
#include "sys/inline.h"
#include "amigarom.h"
#include "amigahr.h"

extern char end[];
extern void figuredisplaytype();
unsigned char getchar();


unsigned long boot_arg0=0, boot_arg1=0; /* Whatever was in d0,d1 from bootstrap */

#define A2620 1				/* A2620/A2630 ROM */
#define EXEC0 2				/* Exec boot method 0 */
#define EXEC1 3				/* Exec boot method 1 */

/*
 * Boot methods:
 *
 * A2620:
 *	D0 = 1
 *	D1 = Nothing
 *	Autoconfig table at 0x78080 (struct acon as per amigarom.h)
 *
 * EXEC0:
 *	D0 = 2
 *	D1 = Pointer to autoconfig table struct ConfigDev[16]
 *		(struct ConfigDev as per libraries/configvars.h)
 *
 * EXEC1:
 *	D0 = 3
 *	D1 = Pointer to struct bootinfo (below)
 *
 */


/*
 * WARNING: config() is called before the big bzero of the .bss section.
 * static variables in this file must be explicitly initialized if they
 * are used or modified by config(), otherwise they get hosed during
 * the big bzero.
 */


/* Define our own struct ConfigDev because AmigaDos header files are silly. */
struct ConfigDev {
	struct	ConfigDev *ln_Succ;
	struct	ConfigDev *ln_Pred;
	unchar	ln_Type;
	char	ln_Pri;
	ushort	ln_Name[2];

	unchar	cd_Flags;
	unchar	cd_Pad;

	/* image of expansion rom area */
	unchar	er_Type;
#define ERTF_MEMLIST            (1<<5)
	unchar	er_Product;
	unchar	er_Flags;
	unchar	er_Reserved03;
	ushort	er_Manufacturer;
	ushort	er_SerialNumber[2];
	ushort	er_InitDiagVec;
	unchar	er_Reserved0c;
	unchar	er_Reserved0d;
	unchar	er_Reserved0e;
	unchar	er_Reserved0f;

	ulong	cd_BoardAddr;		/* where in memory the board is */
	ulong	cd_BoardSize;		/* size in bytes */
	ushort	cd_SlotAddr;		/* which slot number */
	ushort	cd_SlotSize;		/* number of slots the board takes */

	/* AmigaDos stuff */
	ulong	cd_Driver;		/* pointer to node of driver */
	struct ConfigDev *cd_NextCD;	/* linked list of drivers to config */
	ulong	cd_Unused[4];		/* for whatever the driver whats */
};

struct  MemHeader {
	struct	MemHeader *ln_Succ;
	struct	MemHeader *ln_Pred;
	unchar	ln_Type;
	char	ln_Pri;
	ushort	ln_Name[2];
	ushort	mh_Attributes;		/* characteristics of this region */
	struct  MemChunk *mh_First;	/* first free region */
	ulong	mh_Lower;		/* lower memory bound */
	ulong	mh_Upper;		/* upper memory bound */
	ulong	mh_Free;		/* total number of free bytes */
};

struct BootData {
	unsigned long bd_entry;		/* start address */
	unsigned long bd_tvaddr;	/* text start virtual address */
	unsigned long bd_toffset;	/* text segment offset */
	unsigned long bd_tsize;		/* text segment size */
	unsigned long bd_dvaddr;	/* data start virtual address */
	unsigned long bd_doffset;	/* data segment offset */
	unsigned long bd_dsize;		/* data segment size */
};

struct bootinfo {
	struct ConfigDev autocon[NAUTO]; /* Autoconfig devices */
	struct MemHeader memory[NAUTO];	/* Memory regions */
	unsigned char keystates[16];	/* Keyboard state */
	struct BootData bootdata;	/* Info about text & data */
	unsigned char reserved[256];
};
struct bootinfo bootinfo = { { 0 } };


/*
 * Variables
 */
unsigned long	chipmem = 0;		/* Chip memory */

unsigned long MAINSTORE=0, VSIZOFMEM=0;

int kernel_load_address=0;



#ifdef DEBUG
void led_blink(n)
int n;
{
    int i;

    while (n--)
    {
	ACIAA->pra &= ~2;
	for (i=0; i < 0x100000; ++i)
	    ;

	ACIAA->pra |= 2;
	for (i=0; i < 0x100000; ++i)
	    ;
    }
}
#endif /* DEBUG */


/* STUBB */
sysdump(){}


/*
 * Various initialization.  Called from uprt.s before MMU is active
 * and before .bss is zeroed.
 */
config(arg0, arg1)
unsigned long arg0, arg1;		/* Whatever was in d0,d1 from bootstrap */
{
	register int i;

	boot_arg0 = arg0;
	boot_arg1 = arg1;

	/* AmigaDOS Bletcherosity on A3000 */
	*(unsigned char *)0xde0002 |= 0x80;

	/*
	 *  Copy boot information from where the monitor or bootstrap left it.
	 */
	switch (boot_arg0) {
		int nmem;
	case A2620:
		/* Convert JohannROM av_acon[] into ConfigDev */
		nmem = 0;
		for (i=0; i<NAUTO; ++i) {
			bootinfo.autocon[i].cd_BoardAddr    =
				AV->av_acon[i].ac_base;
			bootinfo.autocon[i].cd_BoardSize    =
				AV->av_acon[i].ac_over;
			bootinfo.autocon[i].er_Manufacturer =
				(AV->av_acon[i].ac_pnum) >> 16;
			bootinfo.autocon[i].er_Product      =
				AV->av_acon[i].ac_pnum;
			switch (AV->av_acon[i].ac_pnum) {
			case APNARAM:
			case APNMEMB:
			case APNM020:
			case APNM030:
				bootinfo.memory[nmem].mh_Lower =
					AV->av_acon[i].ac_base;
				bootinfo.memory[nmem].mh_Upper =
					AV->av_acon[i].ac_base +
						AV->av_acon[i].ac_over;
				++nmem;
				break;
			}
		}
		kernel_load_address = 0x200000;
		break;
	case EXEC0:
		bcopy(boot_arg1, bootinfo.autocon, sizeof bootinfo.autocon);
		for ( i=0; i<NAUTO; i++ ) {
			int nmem = 0;
			if (bootinfo.autocon[i].er_Type & ERTF_MEMLIST) {
				bootinfo.memory[nmem].mh_Lower =
					bootinfo.autocon[i].cd_BoardAddr;
				bootinfo.memory[nmem].mh_Upper =
					bootinfo.autocon[i].cd_BoardAddr +
						bootinfo.autocon[i].cd_BoardSize;
				++nmem;
			}
		}
		break;
	case EXEC1:
		bcopy(boot_arg1, &bootinfo, sizeof bootinfo);
		kernel_load_address = bootinfo.bootdata.bd_tvaddr;
		break;
	default:
		chipmem = 0x80000;
		panic("Unknown bootmethod 0x%x/0x%x", boot_arg0, boot_arg1);
	}

	/*
	 * Set master interrupt enable.
	 */
	AMIGA->intena = AINTSET|AIEMAST;

	/*
	 * Very primitive chip RAM sizer.
	 */
	{
		static unsigned long *list[] =
		{ (unsigned long *)0x03fffc,
		  (unsigned long *)0x07fffc,
		  (unsigned long *)0x0ffffc,
		  (unsigned long *)0x1ffffc,
		  (unsigned long *)0,};
		unsigned long **p;

		chipmem = 0;
		for (p=list; *p; ++p)
			**p = 0x5AC3C35A;
		for (p=list; *p; ++p)
			if (**p == 0x5AC3C35A) {
				**p = 0x12345678;
				if (**p == 0x12345678)
					chipmem = (int)((*p)+1);
			}
	}

	/*
	 * Configure main memory.
	 */
	{
		int found;

		MAINSTORE = (unsigned long)end;
		VSIZOFMEM = 0;

		do {
			found = 0;
			for ( i=0; i<NAUTO; i++ ) {
				register unsigned long l, u;
				l = bootinfo.memory[i].mh_Lower;
				u = bootinfo.memory[i].mh_Upper;

				if (l < MAINSTORE &&
				    u >= MAINSTORE) {
					++found;
					MAINSTORE = l;
				}
				if (l <= MAINSTORE+VSIZOFMEM &&
				    u > MAINSTORE+VSIZOFMEM) {
					++found;
					VSIZOFMEM = u-MAINSTORE;
				}
			}
		} while (found);

#define QUICK_KLUDGE
#ifdef QUICK_KLUDGE
		if (!VSIZOFMEM && end > (char *)0x7000000) {
			MAINSTORE = (unsigned long)end & 0xF7C00000;
			VSIZOFMEM = 0x08000000 - MAINSTORE;
		}
#endif /* QUICK_KLUDGE */
	}

	/* Find out whether on PAL system, etc. */
	figuredisplaytype();
}


/*
 * Return information about an autoconfigured device.
 */
int autocon(pc, index, bp, np)
long	pc;
int	index;
long	*bp;
long	*np;
{
	register int i;

	if (index==0 && end > (char *)0x07000000 && (pc==0x0202F003)) {
		*bp = 0xdd0000;
		return 1;
	}

	/*
	 * Search the autoconfig table for the specified board.
	 */
	for (i=0; i<NAUTO; ++i) {
		if (bootinfo.autocon[i].er_Manufacturer != (pc>>16))
			continue;
		if (bootinfo.autocon[i].er_Product != (pc&0xffff))
			continue;
		if (index-- != 0)
			continue;

		*bp = bootinfo.autocon[i].cd_BoardAddr;
		*np = bootinfo.autocon[i].cd_BoardSize;
		return 1;
	}

	return 0;
}


/*
 * Theoretically call the ROM.  Since this is difficult, we wait for a
 * character to be typed in.
 */
callrom()
{
	register int	s;

	s = splhi();
	while (getchar() != '\n')
		;
	splx(s);
}


/*
 * Delay.
 */
delayus(u)
int u;
{
	unsigned int	n;
	unsigned int	a;

	n = 2 + u/(1000000/(264*60));
	while (n-- != 0) {
		a = AMIGA->vhposr & 0xFF00;
		while (a == (AMIGA->vhposr & 0xFF00))
			;
	}
}



extern unsigned char (*congetc)();
extern void (*conputc)();

/*
 * The kernel's routine to get a character from the "system console".
 * congetc is initialized in kernel.c, and usually points to cogetc.
 */
unsigned char getchar()
{
	return (*congetc)();
}


/*
 * The kernel's routine to put out a character on the "system console".
 * (Used by printf, panic, etc.).  conputc is initialized in kernel.c,
 * and usually points to coputc.
 */
void putchar(c)
unsigned char c;
{
	(*conputc)(c);
}
