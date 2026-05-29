/*
 * This file contains the basic functions which deal with the
 * floppy disks.  By basic, I mean below the level of MFM.
 * Note, the inclusion of sys/inline.h and the splx's in fliwrite()
 * are a temporary hack for MS-DOS floppies.
 */
#include "sys/types.h"
#include "sys/inline.h"
#include "amigahr.h"
#include "hstyle.h"
#include "floph.h"


#define	PIGCNT	100			/* index pulse poll count */


/*
 * The typedef spin holds the state of a floppy spindle.
 * If track is unknown, it is set to -1.
 */
typedef struct	spin {
	short	trk;			/* current track */
	bool	on;			/* iff motor on */
}	spin;


/*
 * Local variables.
 * Note, selected is NULL if no drive is selected.
 */
static spin	spindle[4]	= {
			{ -1,	FALSE },
			{ -1,	FALSE },
			{ -1,	FALSE },
			{ -1,	FALSE }
		},
		*selected;		/* currently selected drive */
static uchar	selbits[]	= {
			DSKSEL0, DSKSEL1, DSKSEL2, DSKSEL3
		};


/*
 * Imported functions.
 */
extern void	fldelay();		/* delay a number of mics. */


/*
 * Externally visible functions.
 */
extern void	flinit(),		/* initialize floppy system */
		flstop(),		/* turn floppy motor off */
		fllost();		/* declare floppy lost */
extern bool	flok(),			/* check if floppy in drive */
		flwprot(),		/* check if floppy write protected */
		flread(),		/* read floppy disk */
		flwrite(),		/* write floppy disk */
		fliwrite();		/* write floppy, index sync'd */


/*
 * Local functions.
 */
static bool	motor();		/* turn motor on or off */
static void	select();		/* select a drive */
static bool	seek(),			/* seek to a given track */
		recal();		/* recalibrate drive */


/*
 * Flinit makes sure that the direction (I vs. O) of ACIAA->pra
 * and ACIAB->prb are correct.  It also makes sure that DMA is possible
 * from and to the disk and that all interrupts from the disk are turned
 * off.
 */
void
flinit()
{
	ACIAA->ddra &= ~(DSKRDY | DSKTRACK0 | DSKPROT);
	ACIAB->prb |= DSKSEL;		/* magic juju to avoid id mode */
	ACIAB->prb = ~0;
	ACIAB->ddrb = ~0;
	AMIGA->dmacon = DMASET | 0x210;
	AMIGA->intena = AINTCLR | AIEDSYN | AIEDBLK;
}


/*
 * Flstop turns off the motor of a specified drive.
 */
void
flstop(drv)
uint	drv;
{
	motor(drv, FALSE);
}


/*
 * Fllost declares that a specified drive is not on any known
 * track.
 */
void
fllost(drv)
register uint	drv;
{
	assert(drv < cardof(spindle));
	spindle[drv].trk = -1;
}


/*
 * Return TRUE iff the drive is ok.  Basically, this is the case
 * as long as the drive exists and has a floppy in it.
 */
bool
flok(drv)
register uint	drv;
{
	return (motor(drv, TRUE));
}


/*
 * Return TRUE iff the floppy is write protected.
 */
bool
flwprot(drv)
register uint	drv;
{
	select(drv);
	return ((ACIAA->pra & DSKPROT) == 0);
}


/*
 * Flread reads `len' bytes from track `trk' of drive `drv' into the buffer
 * `buff'.  Note, `buff' must be word aligned and it must be an address as
 * seen from the disk controller point of view (i.e., in [0, 512K) in chip
 * memory.
 * No synchronization is performed, so reading less then a full track is
 * only useful for checking if the drive is on track.
 */
bool
flread(drv, trk, buff, len)
register uint	drv,
		trk;
register char	*buff;
register uint	len;
{
	assert((uint)buff < 2048<<10);
	assert((uint)buff % sizeof(short) == 0);
	assert((uint)len % sizeof(short) == 0);
	assert((uint)len < (1<<14) * sizeof(short));
	if (not motor(drv, TRUE))
		return (FALSE);		/* drive won't spin up */
	select(drv);
	if (not seek(drv, trk))
		return (FALSE);		/* seek failure */
	AMIGA->adkcon = ADKCLR | WORDSYNC | MSBSYNC;
	AMIGA->adkcon = ADKSET | FAST;
	AMIGA->dsklen = 0;
	fldelay(SIDE_SELECT_DEL);	/* magic juju */
	AMIGA->dskpt = (long)buff;
	AMIGA->intreq = AINTCLR | AIEDBLK;
	AMIGA->dsklen = len/sizeof(short) | DMAEN;
	AMIGA->dsklen = len/sizeof(short) | DMAEN;	/* more juju */
	fldelay(len * MPERB);
	while ((AMIGA->intreqr & AIEDBLK) == 0)
		;
	AMIGA->intreq = AINTCLR | AIEDBLK;
	AMIGA->dsklen = 0;
	return (TRUE);
}


/*
 * Flwrite writes `len' bytes from the buffer `buff' to track `trk' of drive
 * `drv'.  Note, `buff' must be word aligned and it must be an address as
 * seen from the disk controller point of view (i.e., in [0, 512K) in chip
 * memory.
 * No synchronization is performed, so writing less then a full track is
 * almost certainly meaningless.
 */
bool
flwrite(drv, trk, buff, len)
register uint	drv,
		trk;
register char	*buff;
register uint	len;
{
	assert((uint)buff < 2048<<10);
	assert((uint)buff % sizeof(short) == 0);
	assert((uint)len % sizeof(short) == 0);
	assert((uint)len < (1<<14) * sizeof(short));
	if (not motor(drv, TRUE))
		return (FALSE);		/* drive won't spin up */
	select(drv);
	if (not seek(drv, trk))
		return (FALSE);		/* seek failure */
	if ((ACIAA->pra & DSKPROT) == 0)
		return (FALSE);		/* disk write protected */
	AMIGA->adkcon = ADKCLR | WORDSYNC | MSBSYNC | PRECOMP1 | PRECOMP0;
	if (trk >= PCOMP3)
		AMIGA->adkcon = ADKSET | FAST | PRECOMP1 | PRECOMP0;
	else if (trk >= PCOMP2)
		AMIGA->adkcon = ADKSET | FAST | PRECOMP1;
	else if (trk >= PCOMP1)
		AMIGA->adkcon = ADKSET | FAST | PRECOMP0;
	else
		AMIGA->adkcon = ADKSET | FAST;
	AMIGA->dsklen = WRITE;
	fldelay(SIDE_SELECT_DEL);	/* magic juju */
	AMIGA->dskpt = (long)buff;
	AMIGA->intreq = AINTCLR | AIEDBLK;
	AMIGA->dsklen = len/sizeof(short) | DMAEN | WRITE;
	AMIGA->dsklen = len/sizeof(short) | DMAEN | WRITE;/* magic juju */
	fldelay(len * MPERB);
	while ((AMIGA->intreqr & AIEDBLK) == 0)
		;
	AMIGA->intreq = AINTCLR | AIEDBLK;
	fldelay(POST_WRITE_DEL);	/* magic juju */
	AMIGA->dsklen = 0;
	return (TRUE);
}


/*
 * Motor is used to turn a drive motor on or off.
 * Note, this may change the selected drive.
 * This routine makes sure that if the motor is off then the
 * drive is NOT selected.  The reason is so that the light on
 * the drive will go off.
 * Motor only returns FALSE if you try to turn a motor on and it
 * does not spin up.
 */
static bool
motor(drv, on)
register uint	drv;
register bool	on;
{
	register spin	*sp;
	register uchar	prb;
	register int	ncnt;

	assert(drv < cardof(spindle));
	sp = &spindle[drv];
	if (sp->on == on) {
		if ((not on)
		and (selected == sp)) {
			ACIAB->prb |= DSKSEL;
			selected = (spin *)NULL;
		}
		return (TRUE);
	}
	prb = ~0;
	if (on)
		prb &= ~DSKMOTOR;
	if (sp->trk % TRKPCYL != 0)
		prb &= ~DSKSIDE;
	ACIAB->prb |= DSKSEL;		/* magic juju to avoid id mode */
	ACIAB->prb = prb;
	prb &= ~selbits[drv];
	ACIAB->prb = prb;
	selected = sp;
	sp->on = on;
	if (on) {
		ncnt = MOTOR_MAX_NAPS;
		while (ACIAA->pra & DSKRDY) {
			if (ncnt-- == 0) {	/* drive won't spin up */
				motor(drv, FALSE);
				return (FALSE);
			}
			fldelay(MOTOR_NAP_DEL);
		}
	} else {
		ACIAB->prb |= DSKSEL;
		selected = (spin *)NULL;
	}
	return (TRUE);
}


/*
 * Select makes sure that the specified drive is selected.
 */
static void
select(drv)
register uint	drv;
{
	register spin	*sp;
	register uint	prb;

	assert(drv < cardof(spindle));
	sp = &spindle[drv];
	if (sp == selected)
		return;
	selected = sp;
	prb = ~0;
	if (sp->trk % TRKPCYL != 0)
		prb &= ~DSKSIDE;
	if (sp->on)
		prb &= ~DSKMOTOR;
	ACIAB->prb |= DSKSEL;		/* magic juju to avoid id mode */
	ACIAB->prb = prb;
	assert(cardof(spindle) == cardof(selbits));
	prb &= ~selbits[drv];
	ACIAB->prb = prb;
}


/*
 * Seek a drive to a track.  It returns TRUE unless the drive
 * fails refused to spin up.
 */
static bool
seek(drv, trk)
register uint	drv,
		trk;
{
	register spin	*sp;
	register uint	prb;
	register int	delta;

	assert(drv < cardof(spindle));
	assert(trk < TRACKS);
	sp = &spindle[drv];
	if (sp->trk == trk)
		return (TRUE);
	if (not motor(drv, TRUE))
		return (FALSE);
	select(drv);
	if ((sp->trk < 0)
	and (not recal(drv)))
		return (FALSE);		/* recal failed */
	delta = sp->trk/TRKPCYL - trk/TRKPCYL;
	prb = ACIAB->prb;
	prb |= DSKSIDE | DSKDIREC;
	if (trk % TRKPCYL != 0)
		prb &= ~DSKSIDE;
	if (delta < 0) {
		delta = - delta;
		prb &= ~DSKDIREC;
	}
	ACIAB->prb = prb;
	if (trk%TRKPCYL != sp->trk%TRKPCYL)
		fldelay(SIDE_SELECT_DEL);
	sp->trk = trk;
	if (delta == 0)
		return (TRUE);
	assert(delta > 0);
	do {
		prb &= ~DSKSTEP;
		ACIAB->prb = prb;
		prb |= DSKSTEP;
		fldelay(1);		/* ha ha */
		ACIAB->prb = prb;
		fldelay(STEP_DEL);
	} while (--delta != 0);
	fldelay(SETTLE_DEL);
	return (TRUE);
}


/*
 * Recal recalibrates the specified drive.
 * It returns TRUE for success.
 * Note the magic juju of stepping in a whole bunch first to try
 * and make the track 0 flag go away.  While stepping in, my experiments
 * indicate that the drive always claims to be at track 0.  I think that
 * the track 0 flag is only accurate when the direction flag is for
 * stepping out.
 */
static bool
recal(drv)
register uint	drv;
{
	register spin	*sp;
	register uint	prb,
			n;

	assert(drv < cardof(spindle));
	sp = &spindle[drv];
	if (not motor(drv, TRUE))
		return (FALSE);		/* drive won't spin up */
	select(drv);
	prb = ACIAB->prb;
	prb |= DSKSIDE;
	prb &= ~DSKDIREC;
	ACIAB->prb = prb;
	for (n = TRACKS/4; n != 0; --n) {
		if (ACIAA->pra & DSKTRACK0)
			break;
		prb &= ~DSKSTEP;
		ACIAB->prb = prb;
		prb |= DSKSTEP;
		fldelay(1);		/* ha ha */
		ACIAB->prb = prb;
		fldelay(STEP_DEL);
	};
	fldelay(SETTLE_DEL);
	prb |= DSKDIREC;
	n = TRACKS/TRKPCYL + RECSLOP;
	loop {
		prb &= ~DSKSTEP;
		ACIAB->prb = prb;
		prb |= DSKSTEP;
		fldelay(1);		/* ha ha */
		ACIAB->prb = prb;
		fldelay(STEP_DEL);
		if ((ACIAA->pra & DSKTRACK0) == 0)
			break;
		if (--n == 0) {		/* if track 0 never true */
			motor(drv, FALSE);
			return (FALSE);
		}
	}
	sp->trk = 0;
	fldelay(SETTLE_DEL);
	return (TRUE);
}


/*
 * Temporary hack to read drive id.
 * Note, this must be fixed to keep select etc. variables up to date.
 */
uint
flid(drv)
int	drv;
{
	uint	res,
		bit,
		nothing,
		selonly,
		motonly,
		selmotor;

	flstop(drv);
	nothing = ~ 0;
	selonly = ~ selbits[drv];
	motonly = ~ DSKMOTOR;
	selmotor = ~ (selbits[drv] | DSKMOTOR);
	ACIAB->prb = nothing;
	ACIAB->prb = motonly;
	ACIAB->prb = selmotor;
	ACIAB->prb = motonly;
	ACIAB->prb = nothing;
	ACIAB->prb = selonly;
	ACIAB->prb = nothing;
	res = 0;
	for (bit = 1<<bitsof(uint)-1; bit != 0; bit >>= 1) {
		ACIAB->prb = selonly;
		if ((ACIAA->pra & DSKRDY) == 0)
			res |= bit;
		ACIAB->prb = nothing;
	}
	selected = (spin *)NULL;
	return (res);
}


/*
 * A temporary hack to write a track synchronized to the index pulse.
 * Other than the synchronization, it is identical to flwrite().
 * Note, the assignment
 *	cnt = ACIAB->icr;
 * rather than just
 *	ACIAB->icr;
 * to clear DSKINDEX is to work around a GNU-CC bug which optimized the
 * latter away even though ACIAB is appropriately volatiled.
 * Note, the hack involving splx's (and the s parameter) is temporary.
 */
bool
fliwrite(drv, trk, buff, len)
uint	drv,
	trk;
char	*buff;
uint	len;
{
	int	s,
		old,
		cnt;
	bool	res;

	unless (motor(drv, TRUE))
		return (FALSE);
	select(drv);
	if (not seek(drv, trk))
		return (FALSE);		/* seek failure */
	loop {
		s = splhi();
		old = AMIGA->intenar & AIEEXTI;
		AMIGA->intena = AINTCLR | AIEEXTI;
		cnt = ACIAB->icr;
		for (cnt = PIGCNT; cnt != 0; --cnt)
			if (ACIAB->icr & DSKINDEX) {
				if (old != 0)
					AMIGA->intena = AINTSET | AIEEXTI;
				res = flwrite(drv, trk, buff, len);
				splx(s);
				return (res);
			}
		if (old != 0)
			AMIGA->intena = AINTSET | AIEEXTI;
		splx(s);
	}
}
