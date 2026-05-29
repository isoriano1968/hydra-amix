#include "sys/types.h"
#include "sys/param.h"
#include "sys/proc.h"
#include "sys/inline.h"
#include "sys/systm.h"
#include "amigahr.h"
#include "memory.h"
#include "screen.h"
#include "copper.h"


static void fixstate(), repeat(), dokb();

/* displayadjust is initialized so it can be patched. */
struct displayadjust displayadjust = { 0, };

struct screen screens[MAXSCREENS];
struct screen *activescreen, *displayedscreen;
struct coplist
    *volatile ExecingCopList,
    *volatile LoadedCopList,
    *volatile QueuedCopList;
static struct coplist nullview;
struct screengroup screengroups[MAXSCREENGROUPS];
unsigned long screenstamp;
int stampwaiting;
struct screen *menuscreen;
#define SCREENCHANGE \
{ ++screenstamp; \
  if (stampwaiting) \
  { stampwaiting=0; wakeup((caddr_t)&stampwaiting); } \
  if (menuscreen) \
    (*menuscreen->kbfunc)(menuscreen, IETYPE_MENU<<24, keyboardstate); \
}
struct screen *blankscreen;		/* Screen to display as screenblanker */
time_t lastevent;			/* Timestamp of last event */
time_t blanktime;			/* Ticks of idle before blanking */

unsigned char mousebuttons;		/* Current state of mouse buttons */
static unsigned char pmx, pmy;		/* Last seen physical mouse counters */
struct keymap *scr_defkmap = &scr_builtinkmap;	/* Pointer to current default kmap */
static unsigned long keyboardstate;	/* Shift keys, prefix keys, etc. */
static unsigned char keystates[16];	/* 1 bit for each possible key */
static unsigned char shiftkeys[16];	/* Bit set for each shiftoid key */
static int timeoutid;			/* Timeout id for keyrepeat */
static unsigned char repcode;		/* Raw code being repeated */
/* kernelkbchar is negative iff kernel is busy-waiting in conget() */
static volatile short kernelkbchar;

static struct font *scr_deffonts[4];	/* Pointers to current default fonts */

static char screeninitflag;


/*
static unsigned short *NextFieldList(coplist)
struct coplist *coplist;
{
    if (coplist->screen && (coplist->screen->modes&Sm_ILACE))
    {
	printf("Loading interlace view,\n");
	printf("  NewCopList 0x%x, NewScreenModes 0x%x,\n",
	       coplist, coplist->screen->modes);
	printf("  ExecingCopList 0x%x, ExecingScreenModes 0x%x\n",
	       ExecingCopList, ExecingCopList->screen->modes);
	printf("  AMIGA->vposr 0x%x 0x%x, NextField is %s\n",
	       AMIGA->vposr, AMIGA->vhposr,
	       ((!(ExecingCopList->screen->modes&Sm_ILACE)^
		 !(AMIGA->vposr&0x8000)) ?
		"SHF" : "LOF" ));
    }
    return _NextFieldList(coplist);
}
*/


/* Debugging counters */
unsigned long
    COP_SAFE_LOADS, COP_UNSAFE_MV, COP_UNSAFE_LV, COP_DEFERRED_RELOAD,
    COP_SAFE_VB, COP_UNSAFE_VB;


void ModifyView(coplist)
struct coplist *coplist;
{
    int s=_spl7();
    if (ExecingCopList == coplist)
	if (!LoadedCopList || LoadedCopList == coplist)
	    if (SAFE_VPOS)
	    {
		AMIGA->cop1lc = (unsigned long)NextFieldList(coplist);
		++COP_SAFE_LOADS;
	    }
	    else
	    {
		coplist->flags |= CopF_NEEDRELOAD;
		++COP_UNSAFE_MV;
	    }
    splx(s);
}


void LoadView(coplist)
struct coplist *coplist;
{
    int s = _spl7();
    BusyCop(coplist);
    if (QueuedCopList)
    {
	FreeCop(QueuedCopList);
	QueuedCopList = 0;
    }
    if (!LoadedCopList && SAFE_VPOS)
    {
	AMIGA->cop1lc = (unsigned long)NextFieldList(LoadedCopList = coplist);
	++COP_SAFE_LOADS;
    }
    else
    {
	QueuedCopList = coplist;
	++COP_UNSAFE_LV;
    }
    (void)splx(s);
}


static void screen_vbint()
{
    int s=splscr();
    register struct screen *sp;

    if (!screeninitflag)
    {
	splx(s);
	return;
    }

    if (LoadedCopList)
    {
	if (ExecingCopList)
	    FreeCop(ExecingCopList);
	ExecingCopList = LoadedCopList;
	LoadedCopList = (struct coplist *)0;
    }

    if (sp = ExecingCopList->screen)
    {
	if (sp->flags & Sf_VBWAIT)
	{
	    if (sp->modes & Sm_HEDLEY)
	    {
		extern unsigned char hedley_fieldnum;
		if (!hedley_fieldnum)
		{
		    sp->flags &= ~Sf_VBWAIT;
		    wakeup((caddr_t)&sp->user);
		}
	    }
	    else
		if (sp->modes & Sm_ILACE)
		{
		    if (AMIGA->vposr&0x8000)
		    {
			sp->flags &= ~Sf_VBWAIT;
			wakeup((caddr_t)&sp->user);
		    }
		}
		else
		{
		    sp->flags &= ~Sf_VBWAIT;
		    wakeup((caddr_t)&sp->user);
		}
	}
	if (sp->flags & Sf_VBCALL)
	    (*sp->mifunc)(sp, SC_VBCALL);
	if (sp->flags & Sf_NEEDCOP)
	    (*sp->mifunc)(sp, SC_BLDCOP);
    }

    (void)_spl7();
    if (SAFE_VPOS)
    {
	if (!LoadedCopList)
	    if (QueuedCopList)
	    {
		AMIGA->cop1lc = (unsigned long)NextFieldList(LoadedCopList = QueuedCopList);
		QueuedCopList = (struct coplist *)0;
	    }
	    else
		if (ExecingCopList->flags&CopF_NEEDRELOAD)
		{
		    AMIGA->cop1lc = (unsigned long)NextFieldList(ExecingCopList);
		    ExecingCopList->flags &= ~CopF_NEEDRELOAD;
		    ++COP_DEFERRED_RELOAD;
		}
	++COP_SAFE_VB;
    }
    else
	++COP_UNSAFE_VB;
    (void)splscr();

    /* Mouse buttons */
    {
	register unsigned char cur_buttons=0, diff;
	register int i;

	if (!(ACIAA->pra & 1<<6))
	    cur_buttons |= MB_0;
	if (!(AMIGA->potinp & 1<<8))
	    cur_buttons |= MB_1;
	if (!(AMIGA->potinp & 1<<10))
	    cur_buttons |= MB_2;

	if (diff = (cur_buttons^mousebuttons))
	{
	    keystates[15] = (keystates[15] & ~(7<<4)) | (cur_buttons<<4);
	    for ( i=0 ; i<3 ; ++i )
		if (diff & 1<<i)
		    dokb((0x7c+i) | (cur_buttons & (1<<i) ? 0 : 0x80), 0);
	    mousebuttons = cur_buttons;
	}
    }

    /* Mouse motion */
    {
	char xrel, yrel;
	unsigned char cur_x, cur_y;

	cur_x = AMIGA->joy0dat;
	cur_y = AMIGA->joy0dat >> 8;
	xrel = cur_x-pmx;
	yrel = cur_y-pmy;
	pmx = cur_x;
	pmy = cur_y;
	if ((xrel || yrel) &&
	    (sp=activescreen))
	{
	    lastevent = lbolt;
	    if (displayedscreen == blankscreen && activescreen != blankscreen)
		DisplayScreen(activescreen);
	    if (sp->kbfunc)
		(*sp->kbfunc)(sp,
			      (0x02ff0000 |
			       ((unsigned char)xrel)<<8 |
			       ((unsigned char)yrel)),
			      ((sp->flags&Sf_RAWKEY)?
			       keystates[12]:keyboardstate));
	}
    }

    if (blankscreen && lbolt > lastevent+blanktime)
	wakeup((caddr_t)&blankscreen);

    splx(s);
}


static void init_copper()
{
    unsigned short *copp = (unsigned short *)AllocMem(16, MEMF_CHIP);
    nullview.LOFlist = nullview.SHFlist = copp;
    move(bplcon0, 1<<15 | 0<<12 | 1<<9); /* Select 0 bitplanes */
    move(color[0], 0);		/* Black background */
    stop();

    BusyCop(&nullview);
    AMIGA->cop1lc = (unsigned long)nullview.LOFlist;
    while (!(AMIGA->intreqr & AIEVSYN))
	;
    AMIGA->intreq = AIEVSYN;
    while (!(AMIGA->intreqr & AIEVSYN))
	;
    AMIGA->intreq = AIEVSYN;
    ExecingCopList = &nullview;
}


void screeninit()
{
    int s;
    if (screeninitflag)
	return;
    s = splscr();

    ++screeninitflag;

    AMIGA->dmacon = DMACLR|DMABLT|DMASPR; /* Turn off blit/sprite DMA */
    AMIGA->dmacon = DMASET|DMABPL|DMACOP; /* Turn on bitplane & copper DMA */

    /* Initialize the blitter and try to put it into a */
    /* consistent state by copying one word onto itself. */
    AMIGA->bltapt = AMIGA->bltbpt = AMIGA->bltcpt = AMIGA->bltdpt = 0;
    AMIGA->bltamod = AMIGA->bltbmod = AMIGA->bltcmod = AMIGA->bltdmod = 0;
    AMIGA->bltafwm = AMIGA->bltafwm = 0xffff;
    AMIGA->bltcon0 = 0x09f0;
    AMIGA->bltcon1 = 0x0000;
    AMIGA->bltsize = 0x0001;		/* This will start the blit.  We */
    /* don't wait for it to finish because the busy bit is broken when */
    /* the power is first turned on, and besides, it's probably done */
    /* already.  The busy bit should work properly after this point. */

    pmx = AMIGA->joy0dat;
    pmy = AMIGA->joy0dat >> 8;

    AMIGA->potgo = 0xff00;		/* Select digital input for mouse buttons */

    /* Enable kbd & vblank interrupts */
    AMIGA->intena = AINTSET|AIEINT2|AIEVSYN;
    ACIAA->icr = ICR_SET|ICR_SP;	/* Enable keyboard interrupt */
    addvbint(screen_vbint, 0L);		/* Add interrupt server.  This ends up */
    /* being the very first vbint server, which is required by some things. */

    init_copper();

    scr_builtinfonts[0]->f_flags |= FF_BUILTIN;
    scr_builtinfonts[1]->f_flags |= FF_BUILTIN;
    scr_builtinfonts[2]->f_flags |= FF_BUILTIN;
    scr_builtinfonts[3]->f_flags |= FF_BUILTIN;

    scr_deffonts[0] = scr_builtinfonts[0];
    scr_deffonts[1] = scr_builtinfonts[1];
    scr_deffonts[2] = scr_builtinfonts[2];
    scr_deffonts[3] = scr_builtinfonts[3];

    splx(s);
}


struct screen *OpenScreen()
{
    register struct screen *sp;
    int s=splscr();

    if (!screeninitflag)
	screeninit();

    for ( sp=screens ; sp<screens+MAXSCREENS ; ++sp )
	if (!(sp->flags & Sf_INUSE))
	{
	    bzero((char *)sp, sizeof (struct screen));
	    sp->flags = Sf_INUSE|Sf_NEEDCOP;
	    SCREENCHANGE;
	    splx(s);
	    return sp;
	}
    splx(s);
    return (struct screen *)0;
}


void CloseScreen(sp)
struct screen *sp;
{
    int s=splscr();

    if (menuscreen == sp)
	menuscreen = 0;
    if (blankscreen == sp)
	blankscreen = 0;

    if (displayedscreen==sp || activescreen==sp)
    {
	register struct screen *nsp;
	for ( nsp=sp->next ; nsp ; ++nsp )
	    if ((nsp->flags & Sf_INUSE) &&
		(nsp->kbfunc) &&
		(nsp->mifunc))
	    {
		DisplayScreen(nsp);
		SelectScreen(nsp);
		break;
	    }
    }

    if (displayedscreen == sp)
	if (menugroup.top && menugroup.top != sp)
	{
	    DisplayScreen(menugroup.top);
	    SelectScreen(menugroup.top);
	}
	else
	{
	    displayedscreen = 0;
	    LoadView(&nullview);
	}
    if (activescreen == sp)
	activescreen = 0;

    if (sp->group)
	UnsetScreenGroup(sp);

    sp->flags = 0;
    if (sp->mifunc)
	(*sp->mifunc)(sp, SC_CHANGE);
    if (sp->kmap && --sp->kmap->km_count <= 0)
	FreeMem(sp->kmap, sp->kmap->km_length);
    {
	int i;
	for ( i=0 ; i<4 ; ++i )
	    if (sp->font[i] &&
		!(sp->font[i]->f_flags & FF_BUILTIN) &&
		--sp->font[i]->f_count <= 0)
		FreeMem(sp->font[i], sp->font[i]->f_length);
    }

    SCREENCHANGE;
    WaitCop(sp->coplist);
    splx(s);
}


int SetScreenGroup(sp, groupnum)
struct screen *sp;
int groupnum;
{
    /* Anonymous groups start at 11 */
    if (groupnum == -1)
	for ( groupnum=11 ; groupnum<MAXSCREENGROUPS ; ++groupnum )
	    if (!screengroups[groupnum].top)
		break;
    if (groupnum < 0 | groupnum >= MAXSCREENGROUPS)
	return -1;

    if (sp->group)
	UnsetScreenGroup(sp);
    sp->group = screengroups+groupnum;
    sp->next = sp->group->top;
    sp->group->top = sp;

    { int s=splscr(); SCREENCHANGE; splx(s); }
    return groupnum;
}


void UnsetScreenGroup(sp)
struct screen *sp;
{
    struct screen **pp = &sp->group->top;
    while ( *pp )
	if (*pp == sp)
	    *pp = sp->next;
	else
	    pp = &(*pp)->next;
    { int s=splscr(); SCREENCHANGE; splx(s); }
}


void SelectScreen(sp)
struct screen *sp;
{
    int s;
    s = splscr();
    if (activescreen)
    {
	if (activescreen == sp)
	{
	    splx(s);
	    return;
	}
	activescreen->flags &= ~Sf_KBD;
	(*activescreen->mifunc)(activescreen, SC_CHANGE);
    }

    keyboardstate = 0;
    activescreen = sp;
    if (timeoutid)
    {
	untimeout(timeoutid);
	timeoutid = 0;
    }
    if (sp)
    {
	fixstate(sp);
	sp->flags |= Sf_KBD;
	(*sp->mifunc)(sp, SC_CHANGE);
    }
    SCREENCHANGE;
    splx(s);
}


void DisplayScreen(sp)
struct screen *sp;
{
    int s;
    s = splscr();

    if (displayedscreen == sp)
    {
	splx(s);
	return;
    }

    if (displayedscreen)
    {
#ifdef maybelater
	if (!LoadedCopList)
	    AMIGA->cop1lc = (unsigned long)nullview.LOFlist;
#endif
	displayedscreen->flags &= ~Sf_DISPLAY;
	(*displayedscreen->mifunc)(displayedscreen, SC_CHANGE);
    }

    if (sp)
    {
	if (!sp->group)
	    if (displayedscreen && displayedscreen->group)
	    {
		sp->group = displayedscreen->group;
		sp->next = sp->group->top;
		sp->group->top = sp;
	    }

	sp->flags |= Sf_DISPLAY;
	(*sp->mifunc)(sp, SC_CHANGE);
	LoadView(sp->coplist);
    }
    else
    {
	LoadView(&nullview);
    }

    displayedscreen = sp;
    SCREENCHANGE;
    splx(s);
}


/* Put screen in front of its group */
/* (i.e., Display/Activate it if its group is the active one.) */
void GroupFront(sp)
struct screen *sp;
{
    int s;
    s = splscr();
    if (!sp->group || displayedscreen && displayedscreen->group == sp->group)
    {
	DisplayScreen(sp);
	SelectScreen(sp);
    }
    splx(s);
}


void HideScreen(sp)
struct screen *sp;
{
    int s=splscr();
    if (displayedscreen==sp || activescreen==sp)
    {
	register struct screen *nsp;
	if (sp->next)
	{
	    DisplayScreen(sp->next);
	    SelectScreen(sp->next);
	    splx(s);
	    return;
	}
	for ( nsp=screens ; nsp<screens+MAXSCREENS ; ++nsp )
	    if ((nsp->flags & Sf_INUSE) &&
		(nsp->kbfunc) &&
		(nsp->mifunc) &&
		(nsp != sp))
	    {
		DisplayScreen(nsp);
		SelectScreen(nsp);
		splx(s);
		return;
	    }
	LoadView(&nullview);
	displayedscreen->flags &= ~Sf_DISPLAY;
	(*displayedscreen->mifunc)(displayedscreen, SC_CHANGE);
	displayedscreen = (struct screen *)0;
	SCREENCHANGE;
    }
    splx(s);
}


void NewCop(sp)
struct screen *sp;
{
    int s=splscr();
    if (displayedscreen == sp)
	LoadView(sp->coplist);
    splx(s);
}


void SetDefFont(fnum, font)
unsigned int fnum;
struct font *font;
{
    int s=splscr();

    if (!(scr_deffonts[fnum]->f_flags & FF_BUILTIN) &&
	--scr_deffonts[fnum]->f_count <= 0)
	FreeMem(scr_deffonts[fnum], scr_deffonts[fnum]->f_length);

    if (font)
    {
	if (!(font->f_flags & FF_BUILTIN))
	    ++font->f_count;
	scr_deffonts[fnum] = font;
    }
    else
	scr_deffonts[fnum] = scr_builtinfonts[fnum];

    splx(s);
}


void SetFont(sp, fnum, font)
struct screen *sp;
unsigned int fnum;
struct font *font;
{
    int s=splscr();

    if (sp->font[fnum] &&
	!(sp->font[fnum]->f_flags & FF_BUILTIN) &&
	--sp->font[fnum]->f_count <= 0)
	FreeMem(sp->font[fnum], sp->font[fnum]->f_length);

    if (!font)
	font = scr_deffonts[fnum];

    if (!(font->f_flags & FF_BUILTIN))
	++font->f_count;
    sp->font[fnum] = font;

    splx(s);
}


#define REPTDELAY	(HZ/2)	/* Initial repeat delay	*/
#define REPTRATE	(HZ/30)	/* Repeat rate */


void SetDefKmap(kmap)
struct keymap *kmap;
{
    int s=splscr();

    if (scr_defkmap)
    {
	if (scr_defkmap != &scr_builtinkmap && --scr_defkmap->km_count <= 0)
	    FreeMem(scr_defkmap, scr_defkmap->km_length);
    }
    if (kmap)
    {
	++kmap->km_count;
	scr_defkmap = kmap;
    }
    else
	scr_defkmap = &scr_builtinkmap;

    if (activescreen && !(activescreen->kmap))
    {
	keyboardstate = 0;
	if (timeoutid)
	{
	    untimeout(timeoutid);
	    timeoutid = 0;
	}
	fixstate(activescreen);
    }

    splx(s);
}


void SetKmap(sp, kmap)
struct screen *sp;
struct keymap *kmap;
{
    int s=splscr();

    if (sp->kmap)
    {
	if (--sp->kmap->km_count <= 0)
	    FreeMem(sp->kmap, sp->kmap->km_length);
    }
    if (kmap)
	++kmap->km_count;
    sp->kmap = kmap;

    if (activescreen == sp)
    {
	keyboardstate = 0;
	if (timeoutid)
	{
	    untimeout(timeoutid);
	    timeoutid = 0;
	}
	fixstate(sp);
    }

    splx(s);
}


struct keymap *GetKmap(sp)
struct screen *sp;
{
    return sp->kmap ? sp->kmap : scr_defkmap;
}


/* Indicate a changed menu-related attribute. */
void TouchScreen(sp)
struct screen *sp;
{
    { int s=splscr(); SCREENCHANGE; splx(s); }
}


static void fixstate(sp)
struct screen *sp;
{
    register struct keymap *kmap;
    register int i, j, k;
    unsigned int mask;

    keyboardstate = 0;
    bzero((caddr_t)shiftkeys, sizeof shiftkeys);

    if (sp && !(sp->flags & Sf_RAWKEY) && sp->kmap)
	kmap = sp->kmap;
    else
	kmap = scr_defkmap;

    for ( mask=1, i=0 ; i<16 ; ++i, mask<<=1 )
	for ( j=0 ; j<4 ; ++j )
	{
	    k = kmap->km_shiftkeys[i][j];
	    if (k>127)
		break;
	    if (keystates[k>>3] & 1<<(k&7))
		keyboardstate |= mask;
	    shiftkeys[k>>3] |= 1<<(k&7);
	}
}


static int encode(kmap, keyent, sp, repeatflag)
struct keymap *kmap;
struct keyent keyent;
struct screen *sp;
int repeatflag;
{
    static char clearprefix;
    register int i;

    switch (keyent.ke_actiontype)
    {
    case KA_CHR:
	if (sp)
	    (*sp->kbfunc)(sp, keyent.ke_value, keyboardstate);
	else
	    if (!(keyent.ke_value&0xffff00))
		kernelkbchar = keyent.ke_value;
	clearprefix=1;
	return 1+!(keyent.ke_flags & KF_NOREP);
    case KA_STR8:
	if (sp)
	{
	    register unsigned char *p = (unsigned char *)
		((unsigned char *)kmap+keyent.ke_value);
	    for ( i= *p++ ; i>0 ; --i )
		(*sp->kbfunc)(sp, *p++, keyboardstate);
	}
	clearprefix=1;
	return 1+!(keyent.ke_flags & KF_NOREP);
    case KA_STR16:
	if (sp)
	{
	    register unsigned short *p = (unsigned short *)
		((unsigned char *)kmap+keyent.ke_value);
	    for ( i= *p++ ; i>0 ; --i )
		(*sp->kbfunc)(sp, *p++, keyboardstate);
	}
	clearprefix=1;
	return 1+!(keyent.ke_flags & KF_NOREP);
    case KA_STR32:
	if (sp)
	{
	    register unsigned long *p = (unsigned long *)
		((unsigned char *)kmap+keyent.ke_value);
	    for ( i= *p++ ; i>0 ; --i )
		(*sp->kbfunc)(sp, *p++, keyboardstate);
	}
	clearprefix=1;
	return 1+!(keyent.ke_flags & KF_NOREP);
    case KA_NOP:
	return 0;
    case KA_STAT:
	if (clearprefix && !repeatflag)
	{
	    keyboardstate &= ~0xff0000;
	    clearprefix=0;
	}
	keyboardstate |= keyent.ke_value;
	return 0;
    case KA_TAB:
	if (clearprefix && !repeatflag)
	{
	    keyboardstate &= ~0xff0000;
	    clearprefix=0;
	}
	{
	    register struct keyent *p = (struct keyent *)
		((unsigned char *)kmap + keyent.ke_value);
	    register unsigned long *mp, m;
	    struct keyent TMP = (*p++);
	    mp = (unsigned long *)
		((unsigned char *)kmap + TMP.ke_value);
	    while (1)
	    {
		m = *mp++;
		if ( (m&keyboardstate) == m )
		    return encode(kmap, *p, sp, repeatflag);
		++p;
	    }
	}
    default:
	printf("invalid keyent 0x%x in keymap.\n", keyent);
	return 0;
    }
}


/*
 * Dispatch a keyboard character.
 */
static void dokb(rawcode, repeatflag)
unsigned char rawcode;			/* Raw command from kbd */
int repeatflag;				/* NZ iff this is a repeat */
{
    register struct keymap *kmap;
    struct screen *sp;
    unsigned char c = rawcode & 0x7f;

    lastevent = lbolt;
    if (displayedscreen == blankscreen && activescreen != blankscreen)
	DisplayScreen(activescreen);

    if ((rawcode&0x80) && timeoutid && repcode == c)
    {
	untimeout(timeoutid);
	timeoutid = 0;
    }

    if (kernelkbchar<0)
    {
	sp = (struct screen *)0,
	kmap = &scr_builtinkmap;
    }
    else
    {
	sp = activescreen;
	if (!sp || !sp->kbfunc)
	    return;
	if (sp->flags & Sf_RAWKEY)
	{
	    (*sp->kbfunc)(sp, 0x01000000|rawcode, keystates[12]);
	    return;
	}

	kmap = sp->kmap;
	if (!kmap)
	    kmap = scr_defkmap;
    }

    /* If this is a shift key, recalulate all keyboardstate bits */
    /* which depend on this key. */
    if (shiftkeys[c>>3] & 1<<(c&7))
    {
	register int i, j, k;
	unsigned int mask;

	for ( mask=1, i=0 ; i<16 ; ++i, mask<<=1 )
	    for ( j=0 ; j<4 ; ++j )
	    {
		k = kmap->km_shiftkeys[i][j];
		if (k>127)
		    break;
		if (k == c)
		{
		    if (rawcode&0x80)
		    {
			keyboardstate &= ~mask;
			for ( j=0 ; j<4 ; ++j )
			{
			    k = kmap->km_shiftkeys[i][j];
			    if (k>127)
				break;
			    if (keystates[k>>3] & 1<<(k&7))
			    {
				keyboardstate |= mask;
				break;
			    }
			}
		    }
		    else
			keyboardstate |= mask;
		    break;
		}
	    }
    }

    switch (encode(kmap,
		   kmap->km_toptable[rawcode],
		   sp, repeatflag))
    {
    case 0:
	break;
    case 1:
	if (timeoutid)
	    untimeout(timeoutid);
	break;
    default:
	if (timeoutid)
	    untimeout(timeoutid);
	if (sp && !(rawcode&0x80))
	{
	    repcode = c;
	    timeoutid = timeout(repeat, 0,
				repeatflag?REPTRATE:REPTDELAY);
	}
    }
}


static void repeat()
{
    dokb(repcode, 1);
}

#define KQ_LSH 0x01
#define KQ_RSH 0x02
#define KQ_CLK 0x04
#define KQ_CTL 0x08
#define KQ_LAL 0x10
#define KQ_RAL 0x20
#define KQ_LAM 0x40
#define KQ_RAM 0x80

#define KB_ACK (1<<6)
/*
 * Keyboard interrupt routine, called (via aciaaintr) during level
 * 2 interrupt only when ACIAA->icr says that the keyboard actually
 * is interrupting.
 */
void kbintr()
{
    register unsigned char rawcode, c, up;
    static unsigned char resetwarning;

    rawcode = ~ACIAA->sdr;
    up = rawcode & 0x01;
    c = rawcode >>= 1;
    if (up)
	rawcode |= 0x80;

    if (rawcode == 0x78 && resetwarning++)
    {
	ACIAA->cra |= KB_ACK;		/* Start handshake. */
	printf("\nKeyboard reset\n");
	dhalt();			/* Let drivers try to shut down. */
	/* Hopefully it won't really kill us until after we halt. */
	delayus(75);
	haltsys(0);			/* haltsys will finish the handshake */
    }

    if (rawcode == 0x5f)
    {
	extern char initarg[];
	extern struct proc *curproc;
	register proc_t	*p;
	for (p = practive; p; p = p->p_next)
	    if (p->p_pid==1)
		break;
	if (!p && !initarg[0])
	{
	    printf("Will boot to single user mode.\n");
	    initarg[0] = 's';
	    initarg[1] = 0;
	}
    }

    /* Start handshake */
    ACIAA->cra |= KB_ACK;

    switch (rawcode)
    {
    case 0x78:
    case 0xf8:
	break;
    case 0xf9:
	/*printf("Keyboard error - retrying\n");*/
	break;
    case 0xfa:
	/*printf("Keyboard output buffer overflow\n");*/
	break;
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
    case 0xff:
	break;

    default:
	if (up)
	    keystates[c>>3] &= ~(1<<(c&7));
	else
	    keystates[c>>3] |= 1<<(c&7);

	{
	    register struct screen *sp;
	    int gn;			/* Group number */

	    if ((keystates[12] & (KQ_LAL|KQ_RAL)))
#ifdef SHIFTY_SNEAKY
		if (((rawcode&0xf0) == 0x50)		&&
		    (keystates[12] & (KQ_LSH|KQ_RSH)))
		{
		    if ((gn = rawcode&0x0f) < MAXSCREENS	&&
			((sp=screens+gn)->flags & Sf_INUSE)	&&
			sp->kbfunc				&&
			sp->mifunc)
		    {
			DisplayScreen(sp);
			SelectScreen(sp);
		    }
		}
		else
#endif /* SHIFTY_SNEAKY */
		    if (((rawcode&0xf0) == 0x50 &&
			 (gn = (rawcode&0x0f)+1) < MAXSCREENGROUPS) ||
			(rawcode == 0x45 &&
			 (gn = 0) < MAXSCREENGROUPS))
		    {
			for ( sp=screengroups[gn].top ; sp ; sp=sp->next )
			    if (sp->kbfunc && sp->mifunc)
				break;
			if (!sp && menugroup.top &&
			    menugroup.top->kbfunc && menugroup.top->mifunc)
			    sp = menugroup.top;
			DisplayScreen(sp);
			SelectScreen(sp);
		    }
	}

	break;
    }

    delayus(75);
    ACIAA->cra &= ~KB_ACK;

    dokb(rawcode, 0);
}


/*
 * Busy-waiting get-a-character-from-the-keyboard routine
 * (for internal kernel use).
 */
static unsigned char conget()
{
    fixstate((struct screen *)0);
    kernelkbchar = -1;
    while (kernelkbchar < 0)
    {
	if (ACIAA->icr&ICR_SP)		/* Keyboard interrupt? */
	    kbintr();
	if (AMIGA->intreqr & AIEVSYN)	/* VB interrupt? */
	    screen_vbint();
    }

    fixstate(activescreen);
    return kernelkbchar;
}

/*
 * The kernel's routine to read a keyboard character.
 * Pointed to by congetc in kernel.c; called by getchar() in support.c.
 */
unsigned char cogetc()
{
    register unsigned char c;

    if ((c=conget()) == '\r')
	c = '\n';
    return c;
}
