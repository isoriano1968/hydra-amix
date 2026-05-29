#include "sys/param.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/vnode.h"
#include "sys/fstyp.h"
#include "sys/file.h"
#include "sys/conf.h"
#include "sys/immu.h"
#include "sys/stream.h"
#include "sys/proc.h"
#include "sys/signal.h"
#include "sys/poll.h"
#include "sys/user.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/inline.h"
#include "amigahr.h"
#include "copper.h"
#include "screen.h"
#include "console.h"
#include "memory.h"

extern void f4cop(), normcop(), hedleyadjust();
extern int check_scrtype();


#define CLONEDEV	0xff		/* Minor number of "clone" device */
#define NSCRDEV	16			/* Number of /dev/screen devices */
#define MAXBM	8			/* Max bitmaps attached to one dev */
#define NIEVENT	100			/* Size of inputevent queue */



static struct scrdev
{
    unsigned short flags;
    unsigned short type;
    struct tty *tty;			/* Unix tty structure */
    struct screen *screen;		/* Screen structure */
    struct bitmap bitmap[MAXBM];	/* Bitmaps */
    struct coplist view[2];		/* Copper lists */
    unsigned short *copbuf[2];		/* Copper list memory */
    unsigned short copsize;		/* Total bytes allocated for each copbuf */
    unsigned short scrflags;		/* Last known screen flags */
    unsigned short width, height;	/* Display size in pixels */
    unsigned short depth;		/* Display depth */
    unsigned long modes;		/* Display modes */
    struct inputevent *ielist;		/* Input event buffer */
    unsigned char ieh, iet;		/* Ielist head & tail */
    struct pollhead pollist;		/* list of all pollers to wake up */
} scrdev[NSCRDEV];

#define DF_OPEN		0x01
#define DF_COPREADY	0x02
#define DF_WAITCOP	0x04
#define DF_CLONEING	0x08		/* Fix race condition */
#define DF_MENUPENDING	0x10


static void scrvb(), scrcop(), scrkb();


static int allocbmap(dp, bp)
register struct scrdev *dp;
register struct bitmap *bp;
{
    int i, size;
    unsigned char *ptr;

    switch (dp->type)
    {
    case 1:
	if (bp->depth > 8)
	{
	    size = bp->width * bp->height * ((bp->depth + 7) / 8);
	    ptr = (unsigned char *)AllocMem(size, MEMF_CHIP|MEMF_PAGEB);
	    if (!ptr)
		return 1;
	    bzero((caddr_t)ptr, size);
	    bp->bpl[0] = ptr;
	    bp->flags = Bf_ACTIVE | Bf_CHUNKY;
	    return 0;
	}
	size = bp->width*bp->height;
	size /= 8;
	for ( i=0 ; i<bp->depth ; ++i )
	{
	    ptr = (unsigned char *)AllocMem(size, MEMF_CHIP|MEMF_PAGEB);
	    if (ptr)
	    {
		bzero((caddr_t)ptr, size);
		bp->bpl[i] = ptr;
	    }
	    else
	    {
		while (i>0)
		    FreeMem(bp->bpl[--i], size);
		return 1;
	    }
	}
	break;
    default:
	return 1;
    }
    bp->flags = Bf_ACTIVE;
    return 0;
}


static void freebmap(dp, bp)
register struct scrdev *dp;
register struct bitmap *bp;
{
    int i, size;

    switch (dp->type)
    {
    case 1:
	if (bp->flags & Bf_CHUNKY)
	{
	    size = bp->width * bp->height * ((bp->depth + 7) / 8);
	    FreeMem(bp->bpl[0], size);
	    break;
	}
	size = bp->width*bp->height;
	size /= 8;
	for ( i=0 ; i<bp->depth ; ++i )
	    FreeMem(bp->bpl[i], size);
	break;
    }
    bp->flags = 0;
}


static void scrmisc(sp, cmd)
struct screen *sp;
int cmd;
{
    register struct scrdev *dp = (struct scrdev *)sp->user;

    switch (cmd)
    {
    case SC_CHANGE:
	if (sp->modes & Sm_CHUNKY)
	{
	    dp->scrflags = sp->flags;
	    break;
	}
	if ((sp->flags & Sf_DISPLAY) &&
	    !(dp->scrflags & Sf_DISPLAY) &&
	    (sp->flags & Sf_NEEDCOP))
	    scrcop(sp);
	dp->scrflags = sp->flags;
	break;
    case SC_VBCALL:
	scrvb(sp);
	break;
    case SC_BLDCOP:
	if (!(sp->modes & Sm_CHUNKY))
	    scrcop(sp);
	break;
    }
}


static void scrcop(sp)
struct screen *sp;
{
    register struct scrdev *dp = (struct scrdev *)sp->user;
    register struct coplist *view;
    unsigned short *copbuf;

    if (!(dp->view[0].flags&CopF_BUSY))
    {
	view = &dp->view[0];
	copbuf = dp->copbuf[0];
    }
    else if (!(dp->view[1].flags&CopF_BUSY))
    {
	view = &dp->view[1];
	copbuf = dp->copbuf[1];
    }
    else
	return;

    ((sp->modes & Sm_HEDLEY) ? f4cop : normcop)
	(sp, view, copbuf, 0);

    view->screen = sp;
    sp->flags &= ~Sf_NEEDCOP;
    sp->coplist = view;
    NewCop(sp);
}


static void scrvb(sp)
register struct screen *sp;
{
    register struct scrdev *dp = (struct scrdev *)sp->user;

    if ((sp->modes & Sm_HEDLEY) &&
	!((sp->flags & Sf_NEEDCOP)))
    {
	register struct scrdev *dp = (struct scrdev *)sp->user;
	register struct coplist *view = sp->coplist;
	hedleyadjust(view, dp->copbuf[view>&dp->view[0]]);
	ModifyView(view);
    }
}


static void scrkb(sp, data, qual)
struct screen *sp;
unsigned long data, qual;
{
    register struct scrdev *dp = (struct scrdev *)sp->user;
    struct inputevent event;

    /* Avoid infinite IETYPE_MENU accumulation */
    if (data == IETYPE_MENU<<24)
    {
	if (dp->flags & DF_MENUPENDING)
	    return;
	dp->flags |= DF_MENUPENDING;
    }

    event.type  = (unsigned char)(data>>24);
    event.class = (unsigned char)(data>>16);
    event.code  = (unsigned short)data;
    event.qualifiers = qual;

    dp->ielist[dp->ieh] = event;

    if (dp->ieh == dp->iet)
	wakeup((caddr_t)&dp->ieh);
    if (dp->pollist.ph_events & POLLRDNORM)
	pollwakeup(&dp->pollist, POLLRDNORM);
    if (dp->pollist.ph_events & POLLIN)
	pollwakeup(&dp->pollist, POLLIN);

    if (++dp->ieh >= NIEVENT)
	dp->ieh = 0;
    if (dp->ieh == dp->iet)
    {
	/* possibly squish the mouse events? */
	if (++dp->iet >= NIEVENT)
	    dp->iet = 0;
    }
}


int scropen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
    register struct scrdev *dp;
    register struct screen *sp;
    int min = getminor(*devp);

    if (min == CLONEDEV)
    {
	for ( min=0 ; min<NSCRDEV ; ++min )
	    if (!scrdev[min].flags)
		break;
	if (min>=NSCRDEV)
	    return ENOSPC;

	*devp = makedevice(getmajor(*devp), min);
    }
    else
	if (min >= NSCRDEV)
	    return ENXIO;
    dp = &scrdev[min];

    if (!(dp->flags & DF_OPEN))
    {
	dp->ielist = (struct inputevent *)
	    AllocMem(NIEVENT * sizeof (struct inputevent), 0);
	if (!dp || !(sp = OpenScreen()))
	    return ENOSPC;
	dp->screen = sp;
	sp->user = (void *)dp;
	dp->scrflags = sp->flags;
	sp->coplist = (struct coplist *)0;
	sp->kbfunc = scrkb;
	sp->mifunc = scrmisc;
	dp->pollist.ph_events = 0;
	dp->pollist.ph_list = NULL;

	dp->flags |= DF_OPEN;
    }

    return 0;
}


int scrclose(dev, flag, type, cr)
dev_t dev;
int flag;
int type;
struct cred *cr;
{
    register struct scrdev *dp = &scrdev[getminor(dev)];
    register struct bitmap *bp;
    register int i;

    CloseScreen(dp->screen);
    for ( i=0, bp=dp->bitmap ; i<MAXBM ; ++i, ++bp )
	if (bp->flags)
	    freebmap(dp, bp);
    if (dp->copbuf[0])
	FreeMem(dp->copbuf[0], dp->copsize*2);
    FreeMem(dp->ielist, NIEVENT * sizeof (struct inputevent));
    bzero((caddr_t)dp, sizeof *dp);

    return 0;
}


int scrread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
    register struct scrdev *dp = &scrdev[getminor(dev)];
    int s = splscr();
    if (dp->ieh == dp->iet && uiop->uio_fmode & FNDELAY)
    {
	splx(s);
	return 0;
    }

    while (dp->ieh == dp->iet)
	sleep((caddr_t)&dp->ieh, PPIPE);

    dp->flags &= ~DF_MENUPENDING;

    while (dp->ieh != dp->iet && uiop->uio_resid >= sizeof (struct inputevent))
    {
	int nev =
	    (dp->ieh > dp->iet) ? (dp->ieh - dp->iet) : NIEVENT - dp->iet;
	nev = min(nev, uiop->uio_resid/sizeof (struct inputevent));
	if (uiomove(dp->ielist+dp->iet, nev * sizeof (struct inputevent), UIO_READ, uiop))
	{
	    splx(s);
	    return EFAULT;
	}
	dp->iet += nev;
	if (dp->iet >= NIEVENT)
	    dp->iet = 0;
    }

    splx(s);
    return 0;
}


int scrwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
    register struct scrdev *dp = &scrdev[getminor(dev)];
    return ENOSPC;
}


int scrioctl(dev, cmd, arg, mode, cr, rvalp)
dev_t dev;
int cmd;
int arg;
int mode;
struct cred *cr;
int *rvalp;
{
    register struct scrdev *dp = &scrdev[getminor(dev)];
    register struct screen *sp = dp->screen;

    switch (cmd)
    {
    case SIOC:
	return 0;

    case SIOCSETDISPLAYADJUST:
	if (copyin((caddr_t)arg, (caddr_t)&displayadjust,
	    sizeof displayadjust))
	    return EFAULT;
	break;
    case SIOCGETDISPLAYADJUST:
	if (copyout((caddr_t)&displayadjust, (caddr_t)arg,
	    sizeof displayadjust))
	    return EFAULT;
	break;

    case SIOCSETGROUP:
	{
	    int ng = SetScreenGroup(sp, arg);
	    if (ng < 0)
		return EINVAL;
	    *rvalp = ng;
	}
	break;
    case SIOCGETGROUP:
	*rvalp = sp->group ? sp->group-screengroups : -1;
	break;

    case SIOCSETINPUTMODE:
	if (arg & ~(SIM_RAWKEY|SIM_MENU))
	    return EINVAL;
	if (arg & SIM_RAWKEY)
	    sp->flags |= Sf_RAWKEY;
	else
	    sp->flags &= ~Sf_RAWKEY;
	if (arg & SIM_MENU)
	    menuscreen = sp;
	break;

    case SIOCDISPLAYTYPE:
	*rvalp = displaytype;
	break;
    case SIOCSETDISPLAYTYPE:
	if ((displaytype & ~DT_CHANGEABLE) != (arg & ~DT_CHANGEABLE))
	    return EINVAL;
	displaytype = (displaytype & ~DT_CHANGEABLE) | (arg & DT_CHANGEABLE);
	if (displaytype & DT_HEDLEY)
	    displaytype |= DT_MONOCHROME;
	break;

    case SIOCSETTYPE:
	{
	    struct scrtype scrtype;
	    if (dp->type)
		return EEXIST;
	    if (copyin((caddr_t)arg, (caddr_t)&scrtype, sizeof scrtype))
		return EFAULT;
	    if (!(displaytype&DT_HEDLEY))
		scrtype.modes |= SM_NONHEDLEY;
	    if (check_scrtype(&scrtype))
		return EINVAL;

	    sp->width = scrtype.dispx;
	    sp->height = scrtype.dispy;
	    sp->depth = scrtype.dispz;
	    sp->modes |=
		((scrtype.modes&SM_HAM ? Sm_HAM : 0) |
		 (scrtype.modes&SM_HALFBRITE ? Sm_HALFBRITE : 0) |
		 (scrtype.modes&SM_INTERLACE ? Sm_ILACE : 0) |
		 (scrtype.modes&SM_HIRES ? Sm_HIRES : 0) |
		 (scrtype.modes&SM_HEDLEY ? Sm_HEDLEY_F4 : 0));

	    dp->type = scrtype.type;

	    dp->copsize = COPSIZE(sp);
	    if (!(dp->copbuf[0] =
		  (unsigned short *)AllocMem(dp->copsize*2, MEMF_CHIP)))
		return ENOMEM;
	    dp->copbuf[1] =
		dp->copbuf[0] + dp->copsize/sizeof (unsigned short);

	    convert_ucolors(sp);

	    if (sp->modes & Sm_HEDLEY)
	    {
		sp->flags |= Sf_VBCALL; /* Make sure we get called every vb */
		sp->modes &= ~Sm_ILACE;
	    }
	}
	break;

    case SIOCGETTYPE:
	{
	    struct scrtype scrtype;
	    if (!dp->type)
		return EINVAL;
	    scrtype.flags = 0;
	    scrtype.type = dp->type;
	    scrtype.dispx = sp->width;
	    scrtype.dispy = sp->height;
	    scrtype.dispz = sp->depth;
	    scrtype.modes =
		((sp->modes&Sm_HAM ? SM_HAM : 0) |
		 (sp->modes&Sm_HALFBRITE ? SM_HALFBRITE : 0) |
		 (sp->modes&Sm_ILACE ? SM_INTERLACE : 0) |
		 (sp->modes&Sm_HIRES ? SM_HIRES : 0) |
		 (sp->modes&Sm_HEDLEY ? SM_HEDLEY : 0));
	    if (copyout((caddr_t)&scrtype, (caddr_t)arg, sizeof scrtype))
		return EFAULT;
	}
	break;

    case SIOCALLOCBMAP:
	{
	    int i;
	    struct bmap bmap;
	    register struct bitmap *bp;
	    if (!dp->type)
		return EINVAL;
	    if (copyin((caddr_t)arg, (caddr_t)&bmap, sizeof bmap))
		return EFAULT;
	    if (bmap.flags)
		return EINVAL;
	    for ( i=0, bp=dp->bitmap ; i<MAXBM && bp->flags ; ++i, ++bp )
		;
	    if (i>=MAXBM)
		return EMFILE;
	    bp->width = bmap.bmapx;
	    bp->height = bmap.bmapy;
	    bp->depth = bmap.bmapz;
	    if (allocbmap(dp, bp))
		return ENOMEM;
	    else
		*rvalp = i;
	}
	break;

    case SIOCSELBMAP:
	{
	    int i = arg & 0xff;
	    int s;
	    struct bitmap *bp = &dp->bitmap[i];
	    if (i >= MAXBM ||
		!dp->type ||
		!bp->flags ||
		bp->width != sp->width ||
		bp->height != sp->height ||
		(bp->depth > MAXBPL && !(sp->modes & Sm_CHUNKY)))
		return EINVAL;
	    s = splscr();
	    if (sp->bmap != bp)
	    {
		sp->bmap = bp;
		if (!(sp->modes & Sm_CHUNKY))
		{
			sp->flags |= Sf_NEEDCOP;
			scrcop(sp);
		}
		else
			sp->flags &= ~Sf_NEEDCOP;
	    }
	    if (arg & SELBMAP_VBWAIT)
	    {
		sp->flags |= Sf_VBWAIT;
		sleep((caddr_t)&sp->user, PZERO+1);
	    }
	    splx(s);
	}
	break;

    case SIOCFRONT:
	if (!sp->bmap)
	    return EINVAL;
	else
	    DisplayScreen(sp);
	break;

    case SIOCBACK:
	HideScreen(sp);
	break;

    case SIOCACTIVATE:
	SelectScreen(sp);
	break;

    case SIOCGROUPFRONT:
	if (!sp->bmap)
	    return EINVAL;
	else
	    GroupFront(sp);
	break;

    case SIOCGETNCOLREGS:
	*rvalp = NUCOLREGS(sp);
	break;

    case SIOCSETCMAP:
	if (!dp->type)
	    return EINVAL;

	if (copyin((caddr_t)arg, (caddr_t)sp->ucolor,
		   NUCOLREGS(sp) * sizeof (struct scolor)))
	    return EFAULT;
	convert_ucolors(sp);
	sp->flags |= Sf_NEEDCOP;
	break;

    case SIOCGETCMAP:
	if (!dp->type)
	    return EINVAL;

	if (copyout((caddr_t)sp->ucolor, (caddr_t)arg,
		    NUCOLREGS(sp) * sizeof (struct scolor)))
	    return EFAULT;
	break;

#ifdef KEYMAPSTUFF
    case SIOCGETKMAP:
	{
	    struct keymap *p = GetKmap(sp);
	    struct keymap km;
	    if (copyin((caddr_t)arg, (caddr_t)&km, sizeof km) ||
		copyout((caddr_t)p, (caddr_t)arg,
			min(km.km_length, p->km_length)))
		return EFAULT;
	}
	break;

    case SIOCSETKMAP:
    case SIOCSETDEFKMAP:
	{
	    struct keymap *p, km;

	    /* User must have r/w permissions to change kmap. */
	    if ((mode&(FREAD|FWRITE)) != (FREAD|FWRITE) &&
		!suser())
		return EPERM;

	    if (arg)
	    {
		if (copyin((caddr_t)arg, (caddr_t)&km, sizeof km))
		    return EFAULT;
		if ((km.km_magic != KM_MAGIC) ||
		    (km.km_length < sizeof km) ||
		    (km.km_length > 0x10000) ||
		    (km.km_count != 0))
		    return ENOEXEC;
		p = (struct keymap *)AllocMem(km.km_length, 0);
		if (!p)
		    return ENOMEM;

		if (copyin((caddr_t)arg, (caddr_t)p, km.km_length))
		{
		    FreeMem(p, km.km_length);
		    return EFAULT;
		}
	    }
	    else
		p = (struct keymap *)0;
	    if (cmd == SIOCSETKMAP)
		SetKmap(sp, p);
	    else
		SetDefKmap(p);
	}
	break;
#endif /* KEYMAPSTUFF */

    case SIOCGETGROUPINFO:
	{
	    struct groupinfo groupinfo;
	    struct screen *sp;
	    int i, n;

	    bzero((caddr_t)&groupinfo, sizeof groupinfo);
	    groupinfo.idstamp = screenstamp;
	    n = 0;
	    for ( i=0 ; i<MAXSCREENGROUPS ; ++i )
		for ( sp=screengroups[i].top ; sp ; sp=sp->next )
		{
		    if (n >= 20)
			break;
		    groupinfo.scr[n].group = i;
		    groupinfo.scr[n].id = sp-screens;
		    strncpy(groupinfo.scr[n].name, sp->name,
			    sizeof groupinfo.scr[n].name);
		    ++n;
		}

	    if (copyout((caddr_t)&groupinfo, (caddr_t)arg, sizeof groupinfo))
		return EFAULT;
	    return 0;
	}

    case SIOCACTIVATESCREEN:
	{
	    struct screen *sp;
	    if (arg < MAXSCREENS &&
		((sp=screens+arg)->flags & Sf_INUSE) &&
		sp->kbfunc && sp->mifunc)
	    {
		DisplayScreen(sp);
		SelectScreen(sp);
		return 0;
	    }
	    else
		return EINVAL;
	}

    case SIOCACTIVATEGROUP:
	{
	    struct screen *sp;
	    if (arg < MAXSCREENGROUPS)
		for ( sp=screengroups[arg].top ; sp ; sp=sp->next )
		    if (sp->kbfunc && sp->mifunc)
		    {
			DisplayScreen(sp);
			SelectScreen(sp);
			return 0;
		    }

	    return EINVAL;
	}

    case SIOCSETNAME:
	if (copyin((caddr_t)arg, (caddr_t)sp->name, SCRNAMESIZE))
	    return EFAULT;
	TouchScreen(sp);
	return 0;

    case SIOCMENUWAIT:
	{
	    int s = splscr();
	    while (screenstamp == arg)
	    {
		stampwaiting = 1;
		sleep((caddr_t)&stampwaiting, PZERO);
	    }
	    splx(s);
	    return 0;
	}

    case SIOCBLANKWAIT:
	if (!dp->type || !sp->bmap)
	    return EINVAL;

	{
	    int s = splscr();
	    if (sp->group)
		UnsetScreenGroup(sp);
	    blankscreen = sp;
	    blanktime = arg*HZ/1000;
	    while (lbolt < lastevent+blanktime)
		sleep((caddr_t)&blankscreen, PZERO+1);
	    if (blankscreen == sp)
		DisplayScreen(sp);
	    splx(s);
	    return 0;
	}

    default:				/* Unknown ioctl command */
	return EINVAL;
    }

    return 0;
}


/*
 * Determines whether the necessary conditions are set on a screen
 * for it to be readable, writeable, or have exceptions.
 */
int scrpoll(dev, events, anyyet, reventsp, phpp)
dev_t dev;
short events;
int anyyet;
short *reventsp;
struct pollhead **phpp;
{
    register short retevents = 0;
    register struct scrdev *dp = &scrdev[getminor(dev)];
    register int s = splscr();

    if (events & POLLWRNORM)
	retevents |= POLLOUT;
    if (dp->ieh != dp->iet)
    {
	if (events & POLLRDNORM)
	    retevents |= POLLRDNORM;
	if (events & POLLIN)
	    retevents |= POLLIN;
    }

    *reventsp = retevents;
    if (retevents)
    {
	splx(s);
	return 0;
    }

    /*
     * If poll() has not found any events yet, set up event cell
     * to wake up the poll if a requested event occurs on this
     * stream.  Check for collisions with outstanding poll requests.
     */
    if (!anyyet)
	*phpp = &dp->pollist;

    splx(s);
    return 0;
}


int scrmmap(dev, offset, maxprot)
dev_t dev;
off_t offset;
int maxprot;
{
    struct scrdev *dp = &scrdev[getminor(dev)];
    register struct bitmap *bp;
    int bmnum, bpnum;

    if (!dp->type)
	return -1/*EINVAL*/;

    bmnum = (offset>>S_BMSHIFT) & S_BMMASK;
    bpnum = (offset>>S_BPSHIFT) & S_BPMASK;
    offset &= S_OFMASK;

    bp = &dp->bitmap[bmnum];
    if (bmnum >= MAXBM || !bp->flags || bpnum >= bp->depth)
	return -1/*EINVAL*/;

    bp->flags |= Bf_MAPPED;

    if (offset < (off_t)(bp->width * bp->height / 8))
	return phystopfn(bp->bpl[bpnum] + offset);

    return -1/*ENXIO*/;
}



