#ifndef _SYS_SCREEN_H
#define _SYS_SCREEN_H

/*
 * <sys/screen.h>
 *
 * At the moment, this file contains two sets of information:
 * 1) the structure definitions and manifest constants needed by
 *    user-level programs,
 * and
 * 2) the declarations for the internal kernel data structures
 *    used by the "screens" kernel subsystem and the device drivers
 *    which use it.
 *
 * This will be split into two files someday.
 *
 */



/* fields in displaytype */
#define DT_NTSC		0x0001			/* NTSC video & clock rate */
#define DT_PAL		0x0002			/* PAL video & clock rate */
#define DT_HEDLEY	0x0004			/* 2024 monitor attached */
#define DT_MONOCHROME	0x0008			/* Monochrome monitor attached */
#define DT_AGNUSVER	0x07f0			/* Agnus version */
#define DTB_AGNUSVER	4			/* Agnus version bit # */
#define DT_DENISEVER	0xff000			/* Denise version */
#define DTB_DENISEVER	12			/* Denise version bit # */

#define DT_CHANGEABLE	(DT_HEDLEY|DT_MONOCHROME) /* Changeable bits */

#define MAXBPL	8


struct displayadjust
{
    short xoffset, yoffset;			/* Screen position */
    unsigned short xoverscan, yoverscan;	/* Max. overscan */
    unsigned short pad[4];
};

#define SCRNAMESIZE	32

#if defined(INKERNEL) || defined(_KERNEL)
extern unsigned int displaytype;
extern struct displayadjust displayadjust;

#define MAXSCREENS 32

#define splscr() spl3()
#endif /* INKERNEL */



/*
 * KEYMAP INFORMATION
 */

struct keyent
{
    unsigned int ke_actiontype:4;
    unsigned int ke_flags:4;
    unsigned int ke_value:24;
};

/* keyent.actiontype */
#define KA_CHR    0			/* Return a character */
#define KA_STR8   1			/* Return a string of 8-bit chars */
#define KA_STR16  2			/* Return a string of 16-bit chars */
#define KA_STR32  3			/* Return a string of 32-bit chars */
#define KA_STAT   5			/* Set a state bit */
#define KA_TAB    6			/* Lookup table */
#define KA_NOP    7			/* Do nothing */

/* keyent.flags */
#define KF_NOREP  0x1			/* Do not repeat */

struct keymap
{
    unsigned short km_magic;			/* Keymap magic number */
    unsigned char km_flags;
    unsigned char km_count;			/* Number of references */
    unsigned long km_length;			/* Total # bytes */
    unsigned char km_shiftkeys[16][4];		/* Definition of shift bits */
    struct keyent km_toptable[256];		/* Top level table */
    /* unsigned char km_tablearea[0]; */	/* Lower tables begin here */
};

/* keymap.km_magic */
#define KM_MAGIC 0x2a4b

#if defined(INKERNEL) || defined(_KERNEL)
extern struct keymap *scr_defkmap, scr_builtinkmap;
extern void SetKmap(), SetDefKmap();
extern struct keymap *GetKmap();
#endif /* INKERNEL */



/*
 * FONT INFORMATION
 *
 * WARNING: the layouts of (struct charent) and (struct font) are also
 * known by the code in font.s and the builtinfonts, neither
 * of which get them from this file!
 */

struct charent
{
    unsigned char ce_height;			/* Height of bitmap */
    unsigned char ce_width;			/* Width of bitmap */
    unsigned char ce_basel;			/* # pixels above baseline */
    unsigned char ce_move;			/* Horiz. cursor increment */
    unsigned char ce_bpl;			/* Bytes per line */
    unsigned char ce_pad;
    unsigned short ce_data;			/* Offset of bitmap data */
};

struct font
{
    unsigned short f_magic;			/* Font file magic number */
    unsigned short f_flags;
    unsigned short f_count;			/* Number of references */
    unsigned short f_pad0;
    unsigned long f_length;			/* Total # bytes */
    unsigned char f_height, f_width;		/* "Typical" character size */
    unsigned char f_baseline;			/* # pixels above baseline */
    unsigned char f_pad1;
    struct charent f_ctable[256];		/* Character table */
};

/* font.f_magic */
#define F_MAGIC 0x2a46

/* font.f_flags */
#define FF_BUILTIN  0x8000

#if defined(INKERNEL) || defined(_KERNEL)
extern struct font *scr_deffonts[4], *scr_builtinfonts[4];
extern void
    SetFont(/*struct screen *sp, int fnum, struct font *fp*/),
    SetDefFont(/*int fnum, struct font *fp*/);
#define GetFont(sp, n) ((sp)->font[n])
#endif /* INKERNEL */



struct scolor
{
    unsigned char gray;				/* 0-255 gray scale value */
    unsigned char red, green, blue;		/* 0-255 RGB */
};

/*
 * MISCELLANEOUS `INKERNEL' INFORMATION
 */

#if defined(INKERNEL) || defined(_KERNEL)

extern unsigned long screenstamp;
extern int stampwaiting;
extern struct screen *menuscreen;
extern time_t lastevent;
extern time_t blanktime;
extern struct screen *blankscreen;

struct coplist
{
    unsigned short flags;
    unsigned short pad0;
    struct screen *screen;
    unsigned short *LOFlist, *SHFlist;
};

#define CopF_BUSY	0x01
#define CopF_WAITING	0x02
#define CopF_ONDISP	0x04
#define CopF_WAITDISP	0x08
#define CopF_NEEDRELOAD	0x10

extern struct coplist
    *volatile ExecingCopList,
    *volatile LoadedCopList,
    *volatile QueuedCopList;

#define BusyCop(cop) ((cop)->flags|=CopF_BUSY)
#define FreeCop(cop) if (((cop)->flags &= ~(CopF_BUSY|CopF_NEEDRELOAD))&CopF_WAITING) \
		     { (cop)->flags &= ~CopF_WAITING; wakeup((caddr_t)(cop)->screen); }
#define WaitCop(cop) while ((cop)->flags & CopF_BUSY) \
		     { (cop)->flags |= CopF_WAITING; sleep((caddr_t)(cop)->screen, PZERO); }
#define NextFieldList(cop) ( (!(ExecingCopList->screen && ExecingCopList->screen->modes&Sm_ILACE)^!(AMIGA->vposr&0x8000)) ? (cop)->LOFlist : (cop)->SHFlist )
#define SAFE_VPOS (!((AMIGA->vhposr>0xf000)||(AMIGA->vposr&0x7)))

#define MAXBPL	8				/* Max bitplanes in a bitmap */

#define Bf_ACTIVE	0x01
#define Bf_MAPPED	0x02
#define Bf_CHUNKY	0x04			/* Chunky pixels */

struct bitmap
{
    unsigned short flags;
    unsigned short width, height, depth, offset;
    unsigned short pad0;
    unsigned char *bpl[MAXBPL];
};

struct screen
{
    unsigned short flags;
    unsigned short modes;			/* Graphics modes, etc. */
    void *user;					/* for driver use */
    unsigned long idx;				/* for driver use */
    struct screengroup *group;			/* Screen group */
    struct screen *next;			/* Next group member */
    void (*mifunc)();				/* Misc function handler */
    void (*kbfunc)();				/* Keyboard handler */
    struct coplist *coplist;			/* Current copper list */
    short hpos, vpos;				/* Position of screen */
    unsigned short height, width, depth;	/* Display size (bits) */
    struct scolor ucolor[32];			/* User Colors */
    unsigned short color[32];			/* Hardware Colors */
    unsigned char drawmode;			/* JAM1, etc. */
    unsigned char fgpen, bgpen;			/* fg/bg colors */
    short row, col;				/* Cursor row and column */
    struct bitmap *bmap;			/* Pointer to current bitmap */
    struct keymap *kmap;			/* Keyboard map */
    struct font *font[4];			/* Pointers to current fonts */
    char name[SCRNAMESIZE];			/* Descriptive name */
};

/* screen.flags */
#define Sf_INUSE	0x0001			/* "open" */
#define Sf_DISPLAY	0x0002			/* On the display */
#define Sf_KBD		0x0004			/* Getting keyboard input */
#define Sf_RAWKEY	0x0100			/* Send "raw" keyboard codes */
#define Sf_NEEDCOP	0x0200			/* Needs a new coplist */
#define Sf_COPUSED	0x0400			/* sp->coplist has been used */
#define Sf_VBCALL	0x0800			/* Do a SC_VBCALL next vb */
#define Sf_COPBUSY	0x1000			/* Coplist is being built */
#define Sf_VBWAIT	0x2000			/* wakeup(&sp->user) next VB */

/* screen.modes */
#define Sm_HIRES	0x0001			/* Amiga hires (width=640) */
#define Sm_ILACE	0x0002			/* Interlaced screen */
#define Sm_SUPERHIRES	0x0004			/* ECS superhires (w=1280) */
#define Sm_FASTSCAN	0x0008			/* ECS productivity (h=480) */
#define Sm_HAM		0x0010			/* Amiga Hold-And-Modify */
#define Sm_HALFBRITE	0x0020			/* Amiga Extra HalfBrite */
#define Sm_HEDLEY_F4	0x0100			/* Hedley-hires screen (F4) */
#define Sm_HEDLEY_F6	0x0200			/* Hedley-hires screen (F6) */
#define Sm_HEDLEY	0x0300			/* Hedley-hires screen (any) */

#define NHCOLREGS(sp) \
    ((sp)->depth>5 ? ((sp)->modes&Sm_HAM ? 16 : 32) : (((sp)->modes&Sm_HEDLEY)?2:1)<<(sp)->depth)
#define NUCOLREGS(sp) \
    ((sp)->depth>5 ? ((sp)->modes&Sm_HAM ? 16 : 32) : 1<<(sp)->depth)

/* For screen.drawmode */
#define JAM1		0		/* jam 1 color into raster */
#define JAM2		1		/* jam 2 colors into raster */
#define COMPLEMENT	2		/* XOR bits into raster */
#define INVERSVID	4		/* inverse video for drawing modes */


/* 'cmd' arg to (*mifunc)() */
#define SC_CHANGE	1			/* Changing DISPLAY/KBD */
#define SC_BLDCOP	2			/* Rebuild copperlist */
#define SC_VBCALL	3			/* Vblank interrupt */


extern struct screen screens[MAXSCREENS];

extern struct screen *OpenScreen(/*void*/);
extern void
    CloseScreen(/*struct screen *sp*/),
    HideScreen(/*struct screen *sp*/),
    SelectScreen(/*struct screen *sp*/),
    DisplayScreen(/*struct screen *sp*/),
    NewCop(/*struct screen *sp*/);
extern void
    ModifyView(/*struct coplist *cp*/);


struct screengroup
{
    struct screen *top;
    char name[42]; /* ??? */
};

#define MAXSCREENGROUPS 20
extern struct screengroup screengroups[MAXSCREENGROUPS];
#define menugroup screengroups[0]

extern int SetScreenGroup(/*struct screen *sp, int groupnum*/);
extern void UnsetScreenGroup(/*struct screen *sp*/);
extern void TouchScreen(/*struct screen *sp*/);


extern unsigned char mousebuttons;
#define MB_0	1				/* Left */
#define MB_1	2				/* Middle */
#define MB_2	4				/* Right */


#endif /* INKERNEL */



/*
 * MISCELLANEOUS USER-LEVEL INFORMATION
 */


#define SCREENMINOR(dev) ((dev)&0xff)

/* Users' ioctl commands and parameters */

#define SIOC		(0xd300)
#define SIOCDISPLAYTYPE	(SIOC|0x01)
#define SIOCFRONT	(SIOC|0x02)
#define SIOCBACK	(SIOC|0x03)
#define SIOCACTIVATE	(SIOC|0x04)
#define SIOCSETKMAP	(SIOC|0x05)
#define SIOCGETKMAP	(SIOC|0x06)
#define SIOCSETDEFKMAP	(SIOC|0x07)
#define SIOCGROUPFRONT	(SIOC|0x08)
#define SIOCGETNCOLREGS	(SIOC|0x09)
#define SIOCSETCMAP	(SIOC|0x0a)
#define SIOCGETCMAP	(SIOC|0x0b)
#define SIOCSETDEFCMAP	(SIOC|0x0c)
#define SIOCSETINPUTMODE	(SIOC|0x0d)
#define SIOCBLANKWAIT		(SIOC|0x0e)
#define SIOCSETDISPLAYTYPE	(SIOC|0x0f)
#define SIOCSETDISPLAYADJUST	(SIOC|0x10)
#define SIOCGETDISPLAYADJUST	(SIOC|0x11)

#define SIOCSETTYPE	(SIOC|0x20)
#define SIOCGETTYPE	(SIOC|0x21)
#define SIOCALLOCBMAP	(SIOC|0x22)
#define SIOCSELBMAP	(SIOC|0x23)
#define SIOCSETGROUP	(SIOC|0x26)
#define SIOCGETGROUP	(SIOC|0x27)
#define SIOCSETNAME	(SIOC|0x28)
#define SIOCGETNAME	(SIOC|0x29)
#define SIOCGETGROUPINFO (SIOC|0x2a)
#define SIOCACTIVATESCREEN (SIOC|0x2b)
#define SIOCACTIVATEGROUP (SIOC|0x2c)
#define SIOCMENUWAIT	(SIOC|0x2d)

#define SIOCSETFONT	(SIOC|0x80)		/* plus font number */
#define SIOCGETFONT	(SIOC|0x90)		/* plus font number */
#define SIOCSETDEFFONT	(SIOC|0xa0)		/* plus font number */


#define	SIOCWINSIZE	(('j'<<8)|5)

struct swinsize					/* results of SIOCWINSIZE */
{
    unsigned char	charsx, charsy;		/* Window size in characters */
    unsigned short	pixsx, pixsy;		/* Window size in pixels */
};

struct inputevent
{
    unsigned char type;
    unsigned char class;
    unsigned short code;
    unsigned long qualifiers;
};

#define IETYPE_ASCII		0
#define IETYPE_RAWKEY		1
#define IETYPE_MOUSE		2
#define IETYPE_MENU		3

#define IECLASS_MOUSEBUTTON	0xfe
#define IECLASS_MOUSEMOTION	0xff

struct scrtype
{
    unsigned short flags;			/* Notes about this request */
    unsigned short type;			/* Should be zero */
    unsigned short dispx, dispy;		/* Display size in pixels */
    unsigned short dispz;			/* Display depth in bits */
    unsigned short pad0;
    unsigned long modes;			/* Display modes (HAM, etc.) */
};

#define ST_ANY		0x00			/* Any screen type */
#define ST_AMIGA	0x01

#define SF_LESSPLANES	0x01			/* Fewer planes are OK */
#define SF_MOREPLANES	0x02			/* More planes are OK */
#define SF_LESSRES	0x04			/* Less resolution is OK */
#define SF_MORERES	0x08			/* More resolution is OK */

#define SM_HAM		0x001			/* Amiga Hold-And-Modify */
#define SM_HALFBRITE	0x002			/* Amiga Extra HalfBrite */
#define SM_INTERLACE	0x004			/* Interlace */
#define SM_NONLACE	0x008			/* Not Interlace */
#define SM_HEDLEY	0x010			/* A2024 Hires mode */
#define SM_NONHEDLEY	0x020			/* Not A2024 Hires mode */
#define SM_HIRES	0x040			/* 640 x ? */
#define SM_LORES	0x080			/* 320 x ? */
#define SM_OVERSCAN	0x100			/* Allow Overscan */
#define SM_NONOVERSCAN	0x200		/* Don't allow Overscan */
#define SM_CHUNKY	0x400		/* Chunky framebuffer mode (16-bit) */
#define SIM_RAWKEY	0x01			/* "Rawkey" input events */
#define SIM_MENU	0x02			/* "Menu" input events */

struct bmap
{
    unsigned short flags;			/* Notes about this request */
    unsigned short bmapx, bmapy;		/* Bitmap size in pixels */
    unsigned short bmapz;			/* Bits per pixel */
};

/* BPADDR() converts bitmap id, bitplane number, and offset (usually zero) */
/* into an address suitable for the mmap() system call */
#define BPADDR(bitmapid,planenum,offset) \
	(((bitmapid)<<S_BMSHIFT)|((planenum)<<S_BPSHIFT)|(offset))
#define S_BMSHIFT 28			/* This leaves 4 bits for bitmapid, */
#define S_BPSHIFT 24			/*  4 bits for planenum, */
#define S_OFSHIFT 0			/*  and 24 bits for offset */
#define S_BMMASK 0xf
#define S_BPMASK 0xf
#define S_OFMASK 0xffffff

#define SELBMAP_VBWAIT	0x10000		/* Wait until VB */


/* for SIOCGETGROUPINFO */
struct groupinfo
{
    long idstamp;
    struct
    {
	short flags;
	short group;
	short id;
	char pad[10];
	char name[SCRNAMESIZE];
    } scr[20];
};


#endif	/* _SYS_SCREEN_H */

/*
Local Variables:
comment-column: 48
End:
*/

