
#define MAXCONSOLES 16

/* For console.flags: */
#define CF_OPEN		0x01		/* This unit is open */
#define CF_INIT		0x02		/* This unit has been SIOCSETTYPEd */
#define CF_NOCURSOR	0x04		/* Cursor temporarily off */

#define MAXARGS 10			/* Maximum # of ansi args */

struct console
{
    unsigned short flags;
    unsigned short pad0;
    struct strtty *tty;			/* Streams tty structure */
    struct screen *screen;		/* Screen structure */
    struct coplist view[2];
    unsigned short *copbuf[2];		/* Copper list memory */
    unsigned short scrflags;		/* Last known screen flags */
    struct bitmap bitmap;		/* The BitMap */
    unsigned short c_attr;		/* Ansi Text attributes */
    unsigned short c_state;		/* Ansi Terminal state */
    unsigned short c_argc, c_curarg;	/* Ansi args */
    unsigned long c_args[MAXARGS];	/* Ansi args */
};


/*
 * ANSI Text attribute information
 */
#define ATTR_BOLD	1
#define ATTR_ITALIC	3
#define ATTR_UNDERSCORE	4
#define ATTR_BLINK	5
#define ATTR_INVERSE	7
#define ATTR_FONT1	8		/* Imaginary */

/*
 * Normal screen parameters.
 */
#define NORMAL_COPFSIZE		128	/* Field coplist size (words) */
#define NORMAL_COPSIZE		((NORMAL_COPFSIZE*2)*2) /* Total size (bytes) */

#define LORES_SCRWIDTH 320
#define HIRES_SCRWIDTH 640
#define NORM_SCRHEIGHT (PAL?256:200)
#define LACE_SCRHEIGHT (PAL?512:400)

/*
 * Hedley screen parameters.
 */
#define HEDLEY_SCRHEIGHT	(PAL?1024:800)
#define HEDLEY_SCRWIDTH		1024
#define HEDLEY_COPFSIZE		128	/* Field coplist size (words) */
#define HEDLEY_COPSIZE		((HEDLEY_COPFSIZE*2)*4) /* Total size (bytes) */


/*
 * Hedley field parameters.
 */
#define	F4_CWIDTH	512		/* field width in pixels */
#define	F4_CHEIGHT	(PAL?512:400)	/* field height in pixels */
#define	F6_CWIDTH	338		/* field width in pixels */
#define	F6_CHEIGHT	(PAL?512:400)	/* field height in pixels */

/*
 * Convert an RGBI value (4 bits) into a color (12 bits).
 * Note, the conversion is:
 *	R -> bit 3 of red
 *	G -> bit 3 of green
 *	B -> bit 3 of blue
 *	I -> bit 0 of blue
 */
#define	rgbi(r, g, b, i)	( (r) << 3+4*2		\
				| (g) << 3+4*1		\
				| (b) << 3+4*0		\
				| (i) << 0+4*0 )


#define COPSIZE(sp) \
	( ((sp)->modes & Sm_HEDLEY) ? HEDLEY_COPSIZE : NORMAL_COPSIZE )

struct bwave
{
    unsigned short flags;
    unsigned short period;
    unsigned short volume;
    unsigned short time;
    unsigned short pad;
    unsigned short nbytes;
    /* char data[0]; */
};

extern void scrcon_putch(/*struct bitmap *bp, short x, short y,
			   struct font *fp,
			   unsigned char c, unsigned char attr*/);
extern void scrcon_cursor(/*struct bitmap *bp, short x, short y,
			    struct font *fp*/);
