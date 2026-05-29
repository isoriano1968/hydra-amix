/*
 * Create Amiga copper programs for normal or "Hedley hires" display.
 */

#include "sys/types.h"
#include "sys/param.h"
#include "amigahr.h"
#include "memory.h"
#include "screen.h"
#include "copper.h"
#include "console.h"

static unsigned char *controlbmap;
static void hedley_field(), f6cop(), norm_field();
void f4cop(), normcop();

#define bitsof(x) ((sizeof (x))*8)

/*
 * Constants for the hedley screen.
 * All horizontal positions in low res pixels.
 *	HSTART	- horizontal start (normal and control).
 *	WSTART	- diwstrt (normal and control).
 *	WSTOP	- diwstop (normal and control).
 *	FSTART	- ddfstrt (normal and control).
 *
 *	NORMST	- normal starting scan line.
 *	NMWIDTH	- normal width.
 *	NMHEND	- normal horizontal end.
 *	NMFSTOP	- normal ddfstop.
 *
 *	CONTSL	- control scan line.
 *	CNWIDTH	- control width.
 *	CNHEND	- control horizontal end.
 *	CNFSTOP	- control ddfstop.
 */
#define	HSTART	0x81
#define	WSTART	(CONTSL<<8 | HSTART)
#define	WSTOP	((NORMST+F4_CHEIGHT/2)<<8 | (HSTART + CNWIDTH/2 - 0x100))
#define	FSTART	0x3C

#define	NORMST	0x2C
#define	NMHEND	(HSTART + F4_CWIDTH/2)
#define	NMFSTOP	(FSTART + 4*(F4_CWIDTH/bitsof(ushort)-2))

#define	CONTSL	(PAL?0x1d:0x15)
#define	CNWIDTH	640
#define	CNHEND	(HSTART + CNWIDTH/2)
#define	CNFSTOP	(FSTART + 4*(CNWIDTH/bitsof(ushort)-2))


/*
 * The following define where the various control regions end
 * (vertically, all are on CONTSL).
 * The positioning unit is low res. pixels (for the wait() copper
 * instruction.
 * CRSKIP is the number of high-res pixels to skip before starting
 * control region 0.
 */
#define	CRSKIP	22			/* pixels of 0 in controlbmap */
#define	CR0	(HSTART + 256/2)	/* end of control region 0 */
#define	CR1	(HSTART + 384/2)	/* end of control region 1 */
#define	CR2	(HSTART + 512/2)	/* end of control region 2 */
#define	CR3	(HSTART + 640/2)	/* end of control region 3 */


/*
 * Compute an address in the bitmap for a given pixel (with y increasing
 * as you go down).
 * Note, this is from the view of the chips.
 */
#define	bmpos(x, y, z)	((sp->bmap->bpl[z]) +		       \
			 (y)*(sp->bmap->width)/8 + \
			 (x)/8)


/*
 * Copper program builder for F4 mode of Hedley high res.
 * One bit plane, non-interlaced.
 * Note, it uses color registers 0-3 as follows:
 *	color[0x0] -> rgbi(0,0,0,0)
 *	color[0x1] -> rgbi(0,0,0,1)
 *	color[0x2] -> rgbi(0,0,1,0)
 *	color[0x3] -> rgbi(0,0,1,1)
 * except during the control regions, when it diddles with color[1].
 * This means that bplpt[0] will go to the I of RGBI and bplpt[1] will
 * go to the B of RGBI.
 */

/* Codes for the first control field:
 *		     +---------------- F6_4
 *		     |  +------------- INTERLACE
 *		     |  |  +---------- Must be 0
 *		     |  |  |  +------- Must be 1
 *		     v  v  v  v
 */
#define F4CODE	rgbi(0, 0, 0, 1)	/* f4 mode code */
#define F6CODE	rgbi(1, 0, 0, 1)	/* f6 mode code */

/* Codes for the second control field:
 *		     +---------------- FN0
 *		     |  +------------- FN1
 *		     |  |  +---------- FN2
 *		     |  |  |  +------- EXPAND
 *		     v  v  v  v
 */
#define	C0CODE	rgbi(0, 0, 0, 1)	/* field 0 code */
#define	C1CODE	rgbi(1, 0, 0, 1)	/* field 1 code */
#define	C2CODE	rgbi(0, 1, 0, 1)	/* field 2 code */
#define	C3CODE	rgbi(1, 1, 0, 1)	/* field 3 code */
#define	C4CODE	rgbi(0, 0, 1, 1)	/* field 4 code */
#define	C5CODE	rgbi(1, 0, 1, 1)	/* field 5 code */


/* Build a copperlist for one "Field" of a "hedley hires" display */

static void hedley_field(sp, copp, code0, code1, xoff, yoff, offset)
struct screen *sp;
register unsigned short *copp; /* Pointer to copperlist buffer */
unsigned short code0, code1;   /* Codes for 1st & 2nd fields in control line */
unsigned short xoff, yoff;     /* Chip address of this chunk of bitmap */
unsigned offset;	       /* # of lines before wrapping to beginning */
{
    int i;

    move(bplcon1, 0);
    move(bplcon2, 7<<3);

    move(dmacon, DMACLR|DMASPR);	/* Turn off sprite DMA */

    /* set up for control line */
    move(diwstrt, WSTART);
    move(diwstop, WSTOP);
    move(ddfstrt, FSTART);
    move(ddfstop, CNFSTOP);
    /* Only need one bitplane for controlbmap */
    move(bplcon0, 1<<15 | 1<<12 | 1<<9);
    movel(bplpt[0], controlbmap);

    /* control info */
    move(color[0], 0);
    move(color[1], code0);
    wait(CONTSL, CR0);
    move(color[1], code1);
    wait(CONTSL, CR1);
    move(color[1], rgbi(1, 1, 0, 0));
    wait(CONTSL, CR2);
    move(color[1], rgbi(1, 1, 0, 1));
    wait(CONTSL, CR3);

    /* skip the unused control lines */
    move(bplcon0, 1<<15 | 0<<12 | 1<<9);
    wait(CONTSL+3, 0);				/* delay */

    /* field data */
    move(ddfstop, NMFSTOP);
    for ( i=0 ; i<1<<(2*sp->depth) ; ++i )
	move(color[i], sp->color[i]);
    move(bpl2mod, (2*sp->bmap->width - F4_CWIDTH) / 8);
    move(bpl1mod, (2*sp->bmap->width - F4_CWIDTH) / 8);

    /* Wait for the data area of the display */
    wait(NORMST-1, NMHEND);
    move(bplcon0, (1<<15 |
		   (sp->depth<<13) |
		   1<<9));

    movel(bplpt[0], bmpos(xoff, yoff, 0));	/* even lines, plane 0 */
    if (offset==1)
	movel(bplpt[1], bmpos(xoff, 0, 0));
    else
	movel(bplpt[1], (bmpos(xoff, yoff, 0)+sp->bmap->width/8));
						/* odd lines, plane 0 */
    if (sp->depth>1)
    {
	movel(bplpt[2], bmpos(xoff, yoff, 1));	/* even lines, plane 1 */
	if (offset==1)
	    movel(bplpt[3], bmpos(xoff, 0, 1));
	else
	    movel(bplpt[3], (bmpos(xoff, yoff, 1)+sp->bmap->width/8));
						/* odd lines, plane 1 */
    }

    if (offset && offset<F4_CHEIGHT)
    {
	int waitline = NORMST+(offset/2);

	if (offset>1)
	{
	    if (waitline >= 256)
	    {
		wait(255, 0x1c1);
		waitline -= 256;
	    }
	    wait(waitline, 0);
	}

	if (offset&1)
	{
	    ++waitline;

	    if (offset>1)
	    {
		movel(bplpt[1], bmpos(xoff, 0, 0));
		if (sp->depth>1)
		    movel(bplpt[3], bmpos(xoff, 0, 1));
	    }

	    if (waitline >= 256)
	    {
		wait(255, 0x1c1);
		waitline -= 256;
	    }
	    wait(waitline, 0);

	    movel(bplpt[0], (bmpos(xoff, 0, 0)+
			     sp->bmap->width/8));
	    if (sp->depth>1)
		movel(bplpt[2], (bmpos(xoff, 0, 1)+
				 sp->bmap->width/8));
	}
	else
	{
	    movel(bplpt[0], bmpos(xoff, 0, 0));
	    movel(bplpt[1], (bmpos(xoff, 0, 0)+
			     sp->bmap->width/8));
	    if (sp->depth>1)
	    {
		movel(bplpt[2], bmpos(xoff, 0, 1));
		movel(bplpt[3], (bmpos(xoff, 0, 1)+
				 sp->bmap->width/8));
	    }
	}
    }
    stop();
}


unsigned char hedley_fieldnum;


void hedleyadjust(view, copbuf)
register struct coplist *view;
unsigned short *copbuf;
{
    ++hedley_fieldnum;
    hedley_fieldnum %= 4;

    view->LOFlist = view->SHFlist =
	copbuf + hedley_fieldnum*HEDLEY_COPFSIZE;
}


/* Build a "hedley hires" F4 mode copperlist */

void f4cop(sp, view, copbuf, offset)
struct screen *sp;
register struct coplist *view;
register unsigned short *copbuf;
unsigned offset;
{
    view->LOFlist = view->SHFlist = copbuf;
    hedley_field(sp, copbuf + 0*HEDLEY_COPFSIZE,
		 F4CODE, C0CODE,
		 0, offset,
		 HEDLEY_SCRHEIGHT-offset);

    if (offset < F4_CHEIGHT)
	hedley_field(sp, copbuf + 2*HEDLEY_COPFSIZE,
		     F4CODE, C1CODE,
		     0, offset+F4_CHEIGHT,
		     F4_CHEIGHT-offset);
    else
	hedley_field(sp, copbuf + 2*HEDLEY_COPFSIZE,
		     F4CODE, C1CODE,
		     0, offset-F4_CHEIGHT,
		     0);

    hedley_field(sp, copbuf + 1*HEDLEY_COPFSIZE,
		 F4CODE, C2CODE,
		 F4_CWIDTH, offset,
		 HEDLEY_SCRHEIGHT-offset);

    if (offset < F4_CHEIGHT)
	hedley_field(sp, copbuf + 3*HEDLEY_COPFSIZE,
		     F4CODE, C3CODE,
		     F4_CWIDTH, offset+F4_CHEIGHT,
		     HEDLEY_SCRHEIGHT/2-offset);
    else
	hedley_field(sp, copbuf + 3*HEDLEY_COPFSIZE,
		     F4CODE, C3CODE,
		     F4_CWIDTH, offset-F4_CHEIGHT,
		     0);
    hedleyadjust(view, copbuf);
}


#if 0
/* Build a "hedley hires" F6 mode copperlist */

static void f6cop(sp, copbuf)
struct screen *sp;
unsigned short *copbuf;
{
    hedley_field(sp, copbuf + 0*HEDLEY_COPFSIZE,
		 F6CODE, C0CODE, 0, 0);

    hedley_field(sp, copbuf + 1*HEDLEY_COPFSIZE,
		 F6CODE, C1CODE, 0, F6_CHEIGHT);

    hedley_field(sp, copbuf + 2*HEDLEY_COPFSIZE,
		 F6CODE, C2CODE, F6_CWIDTH, 0);

    hedley_field(sp, copbuf + 3*HEDLEY_COPFSIZE,
		 F6CODE, C3CODE, F6_CWIDTH, F6_CHEIGHT);

    hedley_field(sp, copbuf + 4*HEDLEY_COPFSIZE,
		 F6CODE, C4CODE, 2*F6_CWIDTH, 0);

    hedley_field(sp, copbuf + 5*HEDLEY_COPFSIZE,
		 F6CODE, C5CODE, 2*F6_CWIDTH, F6_CHEIGHT);
}
#endif


/* Build a copperlist for one field of a "normal" display */

#define WHCENTER	(0x121+displayadjust.xoffset)
#define WVCENTER	((PAL?0xac:0x90)+displayadjust.yoffset)

static void norm_field(sp, copp, nextcopp, flop, offset)
struct screen *sp;
register unsigned short *copp, *nextcopp;
int flop;
unsigned offset;
{
    register int i, modulo;
    unsigned short wvstrt, whstrt, wvstop, whstop, waitline;

    i = (sp->modes&Sm_ILACE) ? sp->height / 2 : sp->height;
    wvstrt = WVCENTER - i/2;
    wvstop = wvstrt + i;

    i = (sp->modes&Sm_HIRES) ? sp->width / 2 : sp->width;
    whstrt = WHCENTER - i/2;
    whstop = whstrt + i;

    waitline = (sp->modes & Sm_ILACE) ?
	(wvstrt+(sp->bmap->height - offset - flop + 1)/2) :
	    (wvstrt+(sp->bmap->height - offset));
    modulo = (sp->modes & Sm_ILACE) ?
	(sp->bmap->width*2 - sp->width)/8 :
	    (sp->bmap->width - sp->width)/8;

    /* Point to coplist for next field */
    if (nextcopp)
	movel(cop1lc, nextcopp);

    move(dmacon, DMACLR|DMASPR);	/* Turn off sprite DMA */

    /* Set window start and stop */
    move(diwstrt, wvstrt<<8 | whstrt);
    move(diwstop, wvstop<<8 | (whstop&0xFF));
    /* Set data fetch start and stop */
    i = (whstrt-(sp->modes&Sm_HIRES?9:17))/2;
    move(ddfstrt, i);
    move(ddfstop, i + sp->bmap->width/(sp->modes&Sm_HIRES?4:2) - 8);

#ifdef OVERSCAN_DEBUG
{
extern struct screen screens[];
if (sp != screens)
{
    printf("screen 0x%x, sh=%d, bh=%d, offset=%d, waitline=%d,\n",
	   sp, sp->height, sp->bmap->height, offset, waitline);
    printf(" sw=%d, bw=%d, modulo=%d\n",
	   sp->width, sp->bmap->width, modulo);
    printf(" wvstrt=0x%x, wvstop=0x%x, whstrt=0x%x, whstop=0x%x\n",
	   wvstrt, wvstop, whstrt, whstop);
    printf(" dfstrt=0x%x, dfstop=0x%x\n",
	   i, i + sp->bmap->width/(sp->modes&Sm_HIRES?4:2) - 8);
}
}
#endif

    for ( i=0 ; i < NHCOLREGS(sp) ; ++i )
	move(color[i], sp->color[i]);

    move(bpl2mod, modulo);
    move(bpl1mod, modulo);

    move(bplcon0, (((sp->modes&Sm_HIRES)?1<<15:0) |
		   ((sp->modes&Sm_HAM)?1<<11:0) |
		   ((sp->modes&Sm_ILACE)?1<<2:0) |
		   (sp->depth<<12) |
		   (1<<9)));
    move(bplcon1, 0<<4 | 0<<0);		/* No hscroll */
    move(bplcon2, 4<<3 | 4<<0);

    wait(wvstrt, 0);
    for ( i=0 ; i < sp->depth ; ++i )
	movel(bplpt[i], (sp->bmap->bpl[i] +
			 (offset+flop)*sp->bmap->width/8));

    if (waitline >= 256)
    {
	wait(255, 0x1c1);
	waitline -= 256;
    }
    wait(waitline, 0);
    for ( i=0 ; i < sp->depth ; ++i )
	movel(bplpt[i], (sp->bmap->bpl[i] +
			 (sp->modes&Sm_ILACE && ((offset+flop)&1)?
			  sp->bmap->width/8:0)));

    stop();
}


/* Build a "normal" copperlist */

void normcop(sp, view, copbuf, offset)
struct screen *sp;
register struct coplist *view;
register unsigned short *copbuf;
unsigned offset;
{
    if (sp->modes & Sm_ILACE)
    {
	view->LOFlist = copbuf;
	view->SHFlist = copbuf+NORMAL_COPFSIZE;
	norm_field(sp,
		   view->LOFlist,
		   view->SHFlist,
		   0, offset);
#ifdef UGLY_DEBUG
	printf("\nField 0:\n");
	dump_coplist(view->LOFlist);
#endif
	norm_field(sp,
		   view->SHFlist,
		   view->LOFlist,
		   1, offset);
#ifdef UGLY_DEBUG
	printf("\nField 1:\n");
	dump_coplist(view->SHFlist);
#endif
    }
    else
    {
	view->LOFlist = view->SHFlist = copbuf;
	norm_field(sp, copbuf, (unsigned short *)0, 0, offset);
    }
}


void conscop(sp)
register struct screen *sp;
{
    register struct console *cp = (struct console *)sp->user;
    register struct coplist *view;
    unsigned short *copbuf;

    if (!(cp->view[0].flags&CopF_BUSY))
    {
	view = &cp->view[0];
	copbuf = cp->copbuf[0];
    }
    else if (!(cp->view[1].flags&CopF_BUSY))
    {
	view = &cp->view[1];
	copbuf = cp->copbuf[1];
    }
    else
	return;

    ((sp->modes & Sm_HEDLEY) ? f4cop : normcop)
	(sp, view, copbuf, sp->bmap->offset);

    view->screen = sp;
    sp->flags &= ~Sf_NEEDCOP;
    sp->coplist = view;
    NewCop(sp);
}


void consvb(sp)
register struct screen *sp;
{
    if ((sp->modes & Sm_HEDLEY) &&
	!((sp->flags & Sf_NEEDCOP)))
    {
	register struct console *cp = (struct console *)sp->user;
	register struct coplist *view = sp->coplist;
	hedleyadjust(view, cp->copbuf[view>&cp->view[0]]);
	ModifyView(view);
    }
}


/*
 * High resolution initialization.
 */
void ahrinit()
{
    register unsigned int i, n;

    controlbmap = (unsigned char *)AllocMem(CNWIDTH/8,
					    MEMF_CHIP);
    for (i = 0; i != CRSKIP/bitsof(ulong); ++i)
	((ulong *)controlbmap)[i] = 0;
    n = CRSKIP - i * bitsof(ulong);
    ((ulong *)controlbmap)[i] = (ulong)~0 >> n;
    while (++i <= CNWIDTH/bitsof(ulong))
	((ulong *)controlbmap)[i] = ~0;
}
