/*
 * Terminal emulator (ANSI variant with variable LINES/COLS).
 * The control characters and escape sequences handled by this driver
 * are:
 *	0x07 (bel)	ring bell
 *	0x08 (backspace)move cursor left one space (if room).
 *	0x09 (tab)	move (not space) to next multiple-of-8 column.
 *	0x0a (line feed)move down one line, scroll if needed.
 *	0x0b (vert tab)	processed as LF
 *	0x0c (form feed)processed as LF
 *	0x0d (crge rtrn)move cursor to first column of line.
 *	Esc [		Control Sequence Introducer
 *	0x9b		Control Sequence Introducer
 *	CSI <N> A	move cursor up N lines.
 *	CSI <N> B	move cursor down N lines.
 *	CSI <N> C	move cursor right N spaces.
 *	CSI <N> D	move cursor left N spaces.
 *	CSI <R>;<C> H	move cursor to row R, column C.
 *	CSI <N> J	erase {to end of line, to end of screen, whole screen}
 *	CSI <N> L	insert N lines.
 *	CSI <N> M	delete N lines.
 *	CSI <N> @	insert N characters.
 *	CSI <N> P	delete N characters.
 *	CSI <N> S	scroll up N lines.
 *	CSI <N> T	scroll down N lines.
 *	CSI 0 m		clear all attributes.
 *	CSI <M> m	activate attribute M.
 *
 * Temporary sequences:
 *	0x0e (shift out)set "FONT1" attribute
 *	0x0f (shift in)	reset "FONT1" attribute
 *
 * Undocumented, unsupported sequences:
 *	CSI <N> a	move cursor up N pixels.
 *	CSI <N> b	move cursor down N pixels.
 *	CSI <N> c	move cursor right N pixels.
 *	CSI <N> d	move cursor left N pixels.
 *	CSI <R>;<C> h	move cursor to pixel (R,C)
 */
#include "sys/types.h"
#include "sys/param.h"
#include "sys/inline.h"
#include "bfinline.h"
#include "amigahr.h"
#include "screen.h"
#include "console.h"
#include "memory.h"

#define	moveu(d,s,n) bcopy((s),(d),(n))
#define zero bzero

#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)>(b))?(b):(a))

#define neednewcoplist(cp) ((cp)->screen->flags |= Sf_NEEDCOP)

/*
 * Someone thinks the following definitions make C more amenable to a purist.
 */
#define	TRUE	1
#define	FALSE	0

#define	nel(a)		(sizeof ( a) / sizeof ( (a)[0]))
#define	endof(a)	(&(a)[nel( a)])

#define	assert(p)

#define	ALIGN	2			/* hardware memory alignment */
#define	TABS	8			/* distance between tab stops */

void revchar();		/* reverse character */

/*
 * Local functions.
 */
static void	bell(),			/* audio beep */
		draw(),			/* draw character */
		ichars(),		/* insert character */
		dchars(),		/* delete character */
		insertbits(),		/* insert pixels */
		deletebits(),		/* delete pixels */
		insert_lines(),		/* insert pixel lines */
		delete_lines(),		/* delete pixel lines */
		doclear(),		/* erase multiple lines */
		char_clear(),		/* erase consecutive chars */
		clear_lines(),		/* clear pixel lines */
		copy_lines(),		/* copy pixel lines */
		moved();		/* move down */


/*
 * Draw places a character in the bitmap.
 */
static void draw(cp, fp, ch)
struct console *cp;
struct font *fp;
unsigned char ch;
{
    register struct screen *sp = cp->screen;

    scrcon_putch(sp->bmap, sp->col, sp->row, fp, ch, cp->c_attr);
}


/*
 * conput handles all terminal emulation.
 */
void scrcon_put(cp, p, n)
register struct console *cp;
register unsigned char *p;
unsigned n;
{
    register unsigned char c;
    register struct screen *sp = (struct screen *)cp->screen;
    register int i;

    if (!(cp->flags & CF_NOCURSOR))
	revchar(cp, 0);

    while ( n )
    {
	--n;
	c = *p++;
	switch (cp->c_state)
	{
	    /*
	     * nothing happening
	     */
	case 0:
	    cp->c_argc = cp->c_curarg = 0;
	    cp->c_args[0] = 0;

	    switch (c)
	    {
	    case 0x07:			/* Bell */
		bell((caddr_t)TRUE);
		break;
	    case 0x08:			/* Backspace */
		if ((sp->col -= sp->font[0]->f_width) < 0)
		{
		    if (sp->row < sp->font[0]->f_height)
			sp->col = 0;
		    else
		    {
			sp->row -= sp->font[0]->f_height;
			sp->col = cp->bitmap.width - sp->font[0]->f_width;
			sp->col -= sp->col % sp->font[0]->f_width;
		    }
		}
		break;
	    case 0x09:			/* Tab */
		sp->col += sp->font[0]->f_width *
		    (TABS - ((sp->col/sp->font[0]->f_width)&(TABS-1)));
		break;
	    case 0x0b:			/* Vertical Tab */
	    case 0x0c:			/* Form Feed */
		/* Implement as LF */
	    case 0x0a:			/* Line feed */
		if ((sp->row += sp->font[0]->f_height) >
		    cp->bitmap.height - sp->font[0]->f_height)
		{
#ifdef MINIMUM_SCROLL
		    delete_lines(cp, 0, (sp->row -
					 (cp->bitmap.height -
					  sp->font[0]->f_height)));
		    sp->row = cp->bitmap.height - sp->font[0]->f_height;
#else
		    delete_lines(cp, 0, sp->font[0]->f_height);
		    sp->row -= sp->font[0]->f_height;
#endif
		}
		break;
	    case 0x0d:			/* Carriage return */
		sp->col = 0;
		break;
	    case 0x0e:			/* SO -- Use font 1 */
		cp->c_attr |= (1<<ATTR_FONT1);
		break;
	    case 0x0f:			/* SI -- Use font 0 */
		cp->c_attr &= ~(1<<ATTR_FONT1);
		break;
	    case 0x1b:			/* Escape ... */
		cp->c_state = 1;
		break;
	    case 0x9b:			/* CSI ... */
		cp->c_state = 2;
		break;
	    default:
		if (c & 0x60)
		{
		    struct font *fp = sp->font[(cp->c_attr&(1<<ATTR_FONT1))?1:0];
		    struct charent *ep = &fp->f_ctable[c];
		    if (sp->col > cp->bitmap.width - ep->ce_move)
		    {
			sp->col = 0;
			sp->row += fp->f_height;
		    }
		    if (sp->row >
			cp->bitmap.height - fp->f_height)
		    {
#ifdef MINIMUM_SCROLL
			delete_lines(cp, 0, (sp->row -
					     (cp->bitmap.height -
					      fp->f_height)));
			sp->row =
			    cp->bitmap.height - fp->f_height;
#else
			delete_lines(cp, 0, fp->f_height);
			sp->row -= fp->f_height;
#endif
		    }
		    draw(cp, fp, c);
		    sp->col += ep->ce_move;
		}
		break;
	    }
	    break;
	    /*
	     * Escape seen.
	     */
	case 1:
	    switch (c)
	    {
	    case '[':			/* Esc [ -> CSI */
		cp->c_state = 2;
		break;
	    default:			/* Esc weird -> ignore */
		cp->c_state = 0;
		break;
	    }
	    break;
	    /*
	     * CSI seen
	     */
	case 2:
	    if (c & 0x40)
	    {
		int N = (cp->c_argc > 0) ? cp->c_args[0] : 1;
		switch (c)
		{
		case 'A':		/* CSI <N> A -> cursor up */
		    N *= sp->font[0]->f_height;
		case 'a':
		    if ((sp->row-=N) < 0)
			sp->row = 0;
		    break;
		case 'B':		/* CSI <N> B -> cursor down */
		    N *= sp->font[0]->f_height;
		case 'b':
		    if ((sp->row+=N) >
			cp->bitmap.height - sp->font[0]->f_height)
			sp->row = cp->bitmap.height - sp->font[0]->f_height;
		    break;
		case 'C':		/* CSI <N> C -> cursor right */
		    N *= sp->font[0]->f_width;
		case 'c':
		    sp->col += N;
		    break;
		case 'D':		/* CSI <N> D -> cursor left */
		    N *= sp->font[0]->f_width;
		case 'd':
		    if ((sp->col -= N) < 0)
			sp->col = 0;	/* Wrap to prev. line? */
		    break;
		case 'H':		/* CSI <N> H -> absolute cursor move */
		    cp->c_args[0] = (cp->c_args[0]-1) * sp->font[0]->f_height;
		    cp->c_args[1] = (cp->c_args[1]-1) * sp->font[0]->f_width;
		case 'h':
		    sp->row = (cp->c_argc>0) ? cp->c_args[0] : 0;
		    if (sp->row > cp->bitmap.height - sp->font[0]->f_height)
			sp->row = cp->bitmap.height - sp->font[0]->f_height;
		    else if (sp->row < 0)
			sp->row = 0;
		    sp->col = (cp->c_argc>1) ? cp->c_args[1] : 0;
		    if (sp->col < 0)
			sp->col = 0;
		    break;
		case 'J':		/* CSI <N> J -> erase */
		    if (cp->c_argc<1)
			N = 0;
		    /* fall through */
		case 'K':		/* CSI <N> K -> erase to end of line */
		    doclear(cp, sp->row, sp->col, N);
		    break;
		case 'T':		/* scroll down N lines. */
		    cp->c_argc = 2;
		    /* fall through */
		case 'L':		/* CSI <N> L -> insert lines */
		    sp->col = 0;
		    N *= sp->font[0]->f_height;
		    {
			int row = cp->c_argc>1 ? 0 : sp->row;
			if (N >= cp->bitmap.height - row)
			    doclear(cp, row, 0, 0);
			else
			{
			    int toclear;
			    insert_lines(cp, row, N);
#ifndef MINIMUM_SCROLL
			    toclear = (cp->bitmap.height - row) %
				sp->font[0]->f_height;
			    if (toclear)
				clear_lines(cp, cp->bitmap.height-toclear, toclear);
#endif
			}
		    }
		    break;
		case 'S':		/* scroll up N lines. */
		    cp->c_argc = 2;
		    /* fall through */
		case 'M':		/* CSI <N> M -> delete lines */
		    /* sp->col = 0; */
		    N *= sp->font[0]->f_height;
		    {
			int row = cp->c_argc>1 ? 0 : sp->row;
			if (N >= cp->bitmap.height - row)
			    doclear(cp, row, 0, 0);
			else
			    delete_lines(cp, row, N);
		    }
		    break;
		case '@':		/* CSI <N> @ -> insert chars */
		    N *= sp->font[0]->f_width;
		    if (N >= cp->bitmap.width - sp->col)
			N = cp->bitmap.width - sp->col;
		    else
			ichars(cp, N);
		    char_clear(cp, sp->row, sp->col, N);
		    break;
		case 'P':		/* CSI <N> P -> delete chars */
		    N *= sp->font[0]->f_width;
		    if (N >= cp->bitmap.width - sp->col)
			N = cp->bitmap.width - sp->col;
		    else
			dchars(cp, N);
		    char_clear(cp, sp->row, cp->bitmap.width-N, N);
		    break;
		case 'm':		/* CSI <N> m */
		    for ( i=0 ; i==0 || i<cp->c_argc ; ++i )
		    {
			N = cp->c_args[i];
			if (N==0)
			    cp->c_attr = 0;
			else if (N>=30 && N<38)
			    sp->fgpen = N-30;
			else if (N>=40 && N<48)
			    sp->bgpen = N-40;
			else
			    cp->c_attr |= 1<<N;
		    }
		    break;
		default:
		    break;
		}
		cp->c_state = 0;
	    }
	    else
	    {
		switch (c)
		{
		case ';':
		    if (cp->c_curarg < MAXARGS-1)
			cp->c_args[++cp->c_curarg] = 0;
		    break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		    if (cp->c_argc == 0)
			cp->c_args[0] = 0;
		    cp->c_argc = cp->c_curarg + 1;
		    cp->c_args[cp->c_curarg] *= 10;
		    cp->c_args[cp->c_curarg] += (c-'0');
		    break;
		case 42:
		    cp->c_state = 0;
		    if ((cp->c_argc>0) && (cp->c_args[0] == '*'))
		    {
			int x=cp->flags;
			static char p[]="\026:;&:90u1'<#0'u7,u\030<>0uw\023:'1wu\021<!!:{";
			if (p[0]==026)
			{
			    char *q=p;
			    while (*q)
				*q++^='U';
			}
			cp->flags |= CF_NOCURSOR;
			scrcon_put(cp, p, sizeof p);
			cp->flags=x;
		    }
		    break;
		default:
		    /* maybe state=0? */
		    break;
		}
	    }
	    break;
	}
    }

    if (!(cp->flags & CF_NOCURSOR))
	revchar(cp, 1);

    if ((sp->flags & Sf_NEEDCOP) &&
	(sp->flags & Sf_DISPLAY))
	conscop(sp);
}


#define INSERTBITS insertbits

/*
 * Ichars makes room for new characters at the current position.
 * It scrolls all characters from that position to the end of the
 * text line <npixels> pixels to the right.
 * Note, (row, col) ... are not cleared.
 */
static void ichars(cp, npixels)
register struct console *cp;
int npixels;
{
    assert(cp->screen->row <= cp->bitmap.height-sp->font[0]->f_height);
    if (cp->screen->col+npixels < cp->bitmap.width)
    {
	register short ln = cp->screen->font[0]->f_height;
	register unsigned char *to;
	int row = cp->screen->row+cp->bitmap.offset;
	if (row >= cp->bitmap.height)
	    row -= cp->bitmap.height;
	to = cp->bitmap.bpl[0] + row * cp->bitmap.width / 8;
	if (row+ln > cp->bitmap.height)
	{
	    while (row < cp->bitmap.height)
	    {
		INSERTBITS(to, cp->screen->col+npixels, cp->screen->col, cp->bitmap.width-cp->screen->col-npixels);
		to += cp->bitmap.width/8;
		++row;
		--ln;
	    }
	    to = cp->bitmap.bpl[0];
	}
	while ( --ln != -1 )
	{
	    INSERTBITS(to, cp->screen->col+npixels, cp->screen->col, cp->bitmap.width-cp->screen->col-npixels);
	    to += cp->bitmap.width/8;
	}
    }
}


#define DELETEBITS deletebits

/*
 * Dchars deletes <npixels> pixels at the current position.
 * It scrolls all characters from <npixels> pixels to the right
 * of that position to the end of the text line <npixels> text
 * positions to the left.
 * Note, (row, COLS-npixels) ... are not cleared.
 */
static void dchars(cp, npixels)
register struct console *cp;
int npixels;
{
    assert(cp->screen->row <= cp->bitmap.height-cp->screen->font[0]->f_height);

    if (cp->screen->col+npixels < cp->bitmap.width)
    {
	register short ln = cp->screen->font[0]->f_height;
	register unsigned char *to;
	int row = cp->screen->row+cp->bitmap.offset;
	if (row >= cp->bitmap.height)
	    row -= cp->bitmap.height;
	to = cp->bitmap.bpl[0] + row * cp->bitmap.width / 8;
	if (row+ln > cp->bitmap.height)
	{
	    while (row < cp->bitmap.height)
	    {
		DELETEBITS(to, cp->screen->col, cp->screen->col+npixels, cp->bitmap.width-cp->screen->col-npixels);
		to += cp->bitmap.width/8;
		++row;
		--ln;
	    }
	    to = cp->bitmap.bpl[0];
	}
	while ( --ln != -1 )
	{
	    DELETEBITS(to, cp->screen->col, cp->screen->col+npixels, cp->bitmap.width-cp->screen->col-npixels);
	    to += cp->bitmap.width/8;
	}
    }
}


/*
 * doclear clears whole character positions.
 * N=0 : row,col to end of screen
 * N=1 : row,col to end of line
 * N=2 : entire screen
 */
static void doclear(cp, row, col, N)
register struct console *cp;
{
    register int i;
    switch (N)
    {
    case 0:				/* to end of screen */
	clear_lines(cp, row+cp->screen->font[0]->f_height,
		    cp->bitmap.height-row-cp->screen->font[0]->f_height);
	/* fall through */
    case 1:				/* to end of line */
	if (col < cp->bitmap.width)
	    char_clear(cp, row, col, cp->bitmap.width-col);
	break;
    case 2:				/* whole screen */
	if (cp->bitmap.offset)
	{
	    cp->bitmap.offset = 0;
	    neednewcoplist(cp);
	}
	for ( i=0 ; i<cp->bitmap.depth ; ++i )
	    zero(cp->bitmap.bpl[i],
		 cp->bitmap.width/8 * cp->bitmap.height);
	break;
    }
}


#define	BLOUD	30
#define	BTIME	(HZ / 4)
#define	BPER	360

struct
{
    struct bwave waveinfo;
    char wavedata[8];
} BWAVE	=
{				/* default bell wave form */
    { 0, BPER, BLOUD, BTIME, 0, 8, },
    0, 90, 127, 90, 0, -90, -127, -90,
};
struct bwave *bwave;			/* Pointer to chip ram for bwave */

/*
 * Bell turns on or off the bell.
 */
static void bell(start)
caddr_t	start;
{
    register int s;
    static time_t ringstop;
    extern time_t lbolt;

    s = splhi();
    if (start)
    {
	if (ringstop)
	{
	    ringstop = lbolt + bwave->time;
	    splx(s);
	    return;
	}
	if (!bwave)
	{
	    /* Copy the bell waveform to chip ram */
	    if (!(bwave = (struct bwave *)
		  AllocMem(sizeof BWAVE, MEMF_CHIP)))
	    {
		splx(s);
		return;
	    }
	    bcopy(&BWAVE, bwave, sizeof BWAVE);
	}
	AMIGA->audio[3].buf = (signed char *)(bwave+1);
	AMIGA->audio[3].size = bwave->nbytes / 2;
	AMIGA->audio[3].period = bwave->period;
	AMIGA->audio[3].loud = bwave->volume;
	AMIGA->dmacon = DMASET | DMAAUD3;
	ringstop = lbolt + bwave->time;
    }
    if (ringstop > lbolt)
	timeout(bell, (caddr_t)FALSE, ringstop-lbolt);
    else
    {
	ringstop = 0;
	AMIGA->dmacon = DMACLR | DMAAUD3;
    }
    splx(s);
}


/*
 * Reverse character (for cursor).
 */
void revchar(cp)
register struct console *cp;
{
    int col = cp->screen->col;

    if (col > cp->bitmap.width - cp->screen->font[0]->f_width)
	col = cp->bitmap.width - cp->screen->font[0]->f_width;
    assert(cp->screen->row <=
	   cp->bitmap.height - cp->screen->font[0]->f_height);
    scrcon_cursor(cp->screen->bmap, col, cp->screen->row, cp->screen->font[0]);
}


/*
 * This is the hairy function which copies a bunch of pixel lines
 * in either direction and figures out how to break the move up
 * into parts which don't "wrap around".  src, dst, and nlines must
 * be within [0,cp->bitmap.height).
 */
static void copy_lines(cp, vdst, vsrc, nlines)
register struct console *cp;
unsigned int vdst, vsrc, nlines;
{
    register unsigned int src, dst;
    register int tmp;

#ifdef UGLY_DEBUG
    printf("Copying %d lines, dst=%d, src=%d, height=%d, offset=%d\n",
	   nlines, vdst, vsrc, cp->bitmap.height, cp->bitmap.offset);
#endif

    src = vsrc + cp->bitmap.offset;
    dst = vdst + cp->bitmap.offset;

    while (nlines > 0)
    {
	if (src >= cp->bitmap.height)
	    src -= cp->bitmap.height;
	if (dst >= cp->bitmap.height)
	    dst -= cp->bitmap.height;

	if (vsrc>vdst)
	{		/* move toward lower addresses */
	    tmp = (cp->bitmap.height - max(src,dst));
	    if (tmp > nlines)
		tmp = nlines;
	    moveu(cp->bitmap.bpl[0] + dst*(cp->bitmap.width/8),
		  cp->bitmap.bpl[0] + src*(cp->bitmap.width/8),
		  tmp*(cp->bitmap.width/8));
	    nlines -= tmp;
	    src += tmp;
	    dst += tmp;
	}
	else
	{				/* move toward higher addresses */
	    tmp = (src+nlines - cp->bitmap.height);
	    if (tmp < 1)
		tmp = (dst+nlines - cp->bitmap.height);
	    if (tmp > 0)
	    {
		nlines -= tmp;
		moved(cp->bitmap.bpl[0] +
		      (dst+nlines)%cp->bitmap.height *
		      (cp->bitmap.width/8),
		      cp->bitmap.bpl[0] +
		      (src+nlines)%cp->bitmap.height *
		      (cp->bitmap.width/8),
		      tmp*(cp->bitmap.width/8));
	    }
	    else
	    {
		moved(cp->bitmap.bpl[0] +
		      dst*(cp->bitmap.width/8),
		      cp->bitmap.bpl[0] +
		      src*(cp->bitmap.width/8),
		      nlines*(cp->bitmap.width/8));
		nlines = 0;
	    }
	}
    }
}


static void insert_lines(cp, line, nlines)
register struct console *cp;
unsigned int line, nlines;
{
    if (line)
	copy_lines(cp, line+nlines, line,
		   cp->bitmap.height-line-nlines);
    else
    {
	cp->bitmap.offset = (cp->bitmap.offset < nlines) ?
	    cp->bitmap.offset-nlines+cp->bitmap.height :
	cp->bitmap.offset-nlines;
	neednewcoplist(cp);
    }
    clear_lines(cp, line, nlines);
}


static void delete_lines(cp, line, nlines)
register struct console *cp;
unsigned int line, nlines;
{
    if (line)
	copy_lines(cp, line, line+nlines,
		   cp->bitmap.height-nlines-line);
    else
    {
	if ( (cp->bitmap.offset += nlines) >= cp->bitmap.height )
	    cp->bitmap.offset -= cp->bitmap.height;
	neednewcoplist(cp);
    }
    clear_lines(cp, cp->bitmap.height-nlines, nlines);
}


/*
 * clear_lines clears entire raster lines.
 */
static void clear_lines(cp, line, nlines)
register struct console *cp;
unsigned int line, nlines;
{
    register unsigned char *to;
    if ((line+=cp->bitmap.offset) >= cp->bitmap.height)
	line -= cp->bitmap.height;
    to = cp->bitmap.bpl[0] + line*cp->bitmap.width/8;

    if (line+nlines > cp->bitmap.height)
    {
	zero(to, (cp->bitmap.height-line)*cp->bitmap.width/8);
	nlines -= cp->bitmap.height-line;
	to = cp->bitmap.bpl[0];
    }
    zero(to, nlines*cp->bitmap.width/8);
}


static void CLEARBITS(line, dstbit, nbits)
unsigned char *line;
unsigned dstbit, nbits;
{
    if (dstbit&7)
    {
	unsigned char mask = 0xff << 8-(dstbit&7);
	if (nbits+(dstbit&7) < 8)
	{
	    mask |= 0xff >> (dstbit&7)+nbits;
	    line[dstbit/8] &= mask;
	    return;
	}

	line[dstbit/8] &= mask;
	nbits -= 8-(dstbit&7);
	dstbit += 8-(dstbit&7);
    }
    zero(line+dstbit/8, nbits/8);
    if (nbits&7)
    {
	unsigned char mask = 0xff >> (nbits&7);
	line[(dstbit+nbits)/8] &= mask;
    }
}


/*
 * Char_clear clears part of a text line.
 */
static void char_clear(cp, crow, ccol, len)
register struct console *cp;
unsigned int crow, ccol, len;
{
    register short sl;
    register unsigned char *to;
    register short ln = cp->screen->font[0]->f_height;

    assert(crow < cp->bitmap.height-cp->screen->font[0]->f_height);
    assert(ccol+len < cp->bitmap.width);

    crow += cp->bitmap.offset;
    if (crow >= cp->bitmap.height)
	crow -= cp->bitmap.height;
    to = cp->bitmap.bpl[0] + crow * cp->bitmap.width / 8;
    if (crow+ln > cp->bitmap.height)
    {
	while (crow < cp->bitmap.height)
	{
	    CLEARBITS(to, ccol, len);
	    to += cp->bitmap.width/8;
	    ++crow;
	    --ln;
	}
	to = cp->bitmap.bpl[0];
    }
    while ( --ln != -1 )
    {
	CLEARBITS(to, ccol, len);
	to += cp->bitmap.width/8;
    }
}


/*
 * Insertbits performs a potentially overlapping downward bitblt within the
 * bitmap line pointed to by "line".
 * Really sleazy implementation using bfext/bfins.
 */
static void insertbits(line, to, from, len)
register unsigned char *line;
register unsigned int to, from, len;
{
    register long x;
    register int bits;
    to += len;
    from += len;
    if (len>=32 && (bits=(to+(unsigned long)line*4)&31))
    {
	to -= bits;
	from -= bits;
	x = bfextu(line, from, bits);
	bfins(line, to, bits, x);
	len -= bits;
    }
    while (len)
    {
	bits = len<32 ? len : 32;
	to -= bits;
	from -= bits;
	x = bfextu(line, from, bits);
	bfins(line, to, bits, x);
	len -= bits;
    }
}



/*
 * Deletebits performs a potentially overlapping upward bitblt within the
 * bitmap line pointed to by "line".
 * Really sleazy implementation using bfext/bfins.
 */
static void deletebits(line, to, from, len)
register unsigned char *line;
register unsigned int to, from, len;
{
    register unsigned long x;
    register int bits;
    if (len>32 && (bits=(-(to+(unsigned long)line*4))&31))
    {
	x = bfextu(line, from, bits);
	bfins(line, to, bits, x);
	len -= bits;
	to += bits;
	from += bits;
    }
    while (len)
    {
	bits = len>32 ? 32 : len;
	x = bfextu(line, from, bits);
	bfins(line, to, bits, x);
	len -= bits;
	to += bits;
	from += bits;
    }
}


/*
 * Moved moves memory down.
 */
static void moved(to, from, len)
register unsigned char *to, *from;
register unsigned int len;
{
    register unsigned int n;

    from += len;
    to += len;
    if (((unsigned)from%ALIGN != (unsigned)to%ALIGN) ||
	(len < 2*sizeof (int)))
    {
	while (len--)
	    *--to = *--from;
	return;
    }
    while ((unsigned)from%ALIGN)
    {
	*--to = *--from;
	--len;
    }
    n = len >> 2;			/* C BUG */
    assert(n > 0);
    do
    {
	to -= sizeof (int);
	from -= sizeof (int);
	*(int *)to = *(int *)from;
    }
    while (--n != 0);
    len %= sizeof (int);
    while (len--)
	*--to = *--from;
}
