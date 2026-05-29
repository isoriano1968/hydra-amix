#include "sys/param.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/vnode.h"
#include "sys/fstyp.h"
#include "sys/file.h"
#include "sys/conf.h"
#include "sys/immu.h"
#include "sys/proc.h"
#include "sys/signal.h"
#include "sys/user.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/inline.h"
#include "ben.h"

volatile clock_registers_t * const clockbase = CLOCKBASE;

enum CLOCK_TYPE clock_type;

#define	SECONDS_PER_MINUTE	(60)
#define MINUTES_PER_HOUR	(60)
#define HOURS_PER_DAY		(24)
#define SECONDS_PER_HOUR	(SECONDS_PER_MINUTE * MINUTES_PER_HOUR)
#define SECONDS_PER_DAY		((long) SECONDS_PER_HOUR * HOURS_PER_DAY)

#define	dysize(A) (((A)%4)? 365: 366)	/* number of days per year */

static int dmsize[12]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static void
oki_write(clkx)
struct ben *clkx;
{
    clockbase->Reg0 = clkx->hsecs%10;
    clockbase->Reg1 = clkx->hsecs/10;
    clockbase->Reg2 = clkx->hmins%10;
    clockbase->Reg3 = clkx->hmins/10;
    clockbase->Reg4 = clkx->hhours%10;
    clockbase->Reg5 = clkx->hhours/10;
    clockbase->Reg6 = clkx->hdays%10;
    clockbase->Reg7 = clkx->hdays/10;
    clockbase->Reg8 = clkx->hmonth%10;
    clockbase->Reg9 = clkx->hmonth/10;
    clockbase->RegA = clkx->hyear%10;
    clockbase->RegB = clkx->hyear/10;
    clockbase->RegC = clkx->hweekday - 1;
}

static void
ricoh_write(clkx)
struct ben *clkx;
{
    clockbase->RegD = (Ricoh_TIMER_EN|Ricoh_M00);

    clockbase->Reg0 = clkx->hsecs%10;
    clockbase->Reg1 = clkx->hsecs/10;
    clockbase->Reg2 = clkx->hmins%10;
    clockbase->Reg3 = clkx->hmins/10;
    clockbase->Reg4 = clkx->hhours%10;
    clockbase->Reg5 = clkx->hhours/10;
    clockbase->Reg6 = clkx->hweekday - 1;
    clockbase->Reg7 = clkx->hdays%10;
    clockbase->Reg8 = clkx->hdays/10;
    clockbase->Reg9 = clkx->hmonth%10;
    clockbase->RegA = clkx->hmonth/10;
    clockbase->RegB = clkx->hyear%10;
    clockbase->RegC = clkx->hyear/10;

    clockbase->RegD = (Ricoh_TIMER_EN|Ricoh_M01);
}

static void
clock_write(clkx)
struct ben *clkx;
{
    switch ((int)clock_type)
    {
    default:
    case RICOH:
	ricoh_write(clkx);
	break;

    case OKI:
	oki_write(clkx);
	break;
    }
}

static void
oki_reset()
{
    clockbase->RegD = Oki_IRQ;
    clockbase->RegF = (Oki_2112|Oki_STOP|Oki_REST); /* reset */
    clockbase->RegF &= ~(Oki_STOP|Oki_REST); /* restart */
    clockbase->RegE = 0;

    clockbase->RegF = Oki_REST; /* reset */
    clockbase->RegF |= Oki_2112; /* set 24 bit */
    clockbase->RegF &= ~Oki_REST; /* restart */

    printf("HARDWARE CLOCK [Oki]: Bad Date.  Please set the clock manually.\n");
}

/*
** Polynomial => X^8 + X^6 + X^4 + X^2 + X + 1
*/
static unsigned char CRCTable[] =
{
    0x00, 0x57, 0xae, 0xf9,
    0x0b, 0x5c, 0xa5, 0xf2,
    0x16, 0x41, 0xb8, 0x7f,
    0x1d, 0x4a, 0xb3, 0xe4,
};

static unsigned char
divnibble(data, crc)
unsigned char data, crc;
{
    data = (data&0xF0) ^ crc;
    data = ((data >> 4)&0x0F) | ((data << 4)&0xF0);
    crc = data&0xF0;
    crc ^= CRCTable[data&0x0F];

    return crc;
}

static unsigned char
calcCRC(addr, size)
volatile unsigned char *addr;
int size;
{
    unsigned char crc = (unsigned char)-1;

    while (size--)
    {
	crc = divnibble(*addr, crc);
	crc = divnibble(*addr++ << 4, crc);
    }

    return crc;
}

static void
ricoh_reset()
{
    clockbase->RegE = 0;
    clockbase->RegD = Ricoh_M01;
    clockbase->RegF = (Ricoh_TIMER_RESET|Ricoh_TIMER_RESET);
    clockbase->RegF &= ~(Ricoh_TIMER_RESET|Ricoh_TIMER_RESET);

    clockbase->RegA = 1;	/* set 24 hour bit */

    clockbase->RegD |= Ricoh_TIMER_EN;

#define RicohMemSize	(13)
    *(((volatile unsigned char *)clockbase)+RicohMemSize-1) =
	calcCRC((volatile unsigned char *)clockbase, RicohMemSize-1);

    printf("HARDWARE CLOCK [Ricoh]: Bad Date.  Please set the clock manually.\n");
}

static int
check_clock(clkx)
struct ben *clkx;
{
#ifdef DEBUG
    printf("ClockType: %s; %d %d %d %d %d [%d]\n",
	   clock_type == OKI ? "Oki" : "RicoH",
	   clkx->hmonth, clkx->hdays, clkx->hhours, clkx->hmins, clkx->hyear,
	   clkx->hweekday);
#endif

    if (clkx->hsecs    < 0 || clkx->hsecs    >= 60 ||
	clkx->hmins    < 0 || clkx->hmins    >= 60 ||
	clkx->hhours   < 0 || clkx->hhours   >= 24 ||
	clkx->hdays    < 1 || clkx->hdays    > 31  ||
	clkx->hweekday < 1 || clkx->hweekday > 8   ||
	clkx->hmonth   < 1 || clkx->hmonth   > 12)
    {
	switch ((int)clock_type)
	{
	default:		/* our default clock for now */
	case RICOH:
	    ricoh_reset();
	    break;

	case OKI:
	    oki_reset();
	    break;
	}

	return 0;
    }

   return 1;
}

static void
oki_read(clkx)
struct ben *clkx;
{
    clkx->hsecs =
	    (VALIDBITS(clockbase->Reg1) * 10) + VALIDBITS(clockbase->Reg0);
    clkx->hmins =
	    (VALIDBITS(clockbase->Reg3) * 10) + VALIDBITS(clockbase->Reg2);
    clkx->hhours =
	    (VALIDBITS(clockbase->Reg5) * 10) + VALIDBITS(clockbase->Reg4);
    clkx->hdays =
	    (VALIDBITS(clockbase->Reg7) * 10) + VALIDBITS(clockbase->Reg6);
    clkx->hmonth =
	    (VALIDBITS(clockbase->Reg9) * 10) + VALIDBITS(clockbase->Reg8);
    clkx->hyear =
	    (VALIDBITS(clockbase->RegB) * 10) + VALIDBITS(clockbase->RegA);
    clkx->hweekday = VALIDBITS(clockbase->RegC) + 1;
}

static void
ricoh_read(clkx)
struct ben *clkx;
{
    clockbase->RegD = (Ricoh_TIMER_EN|Ricoh_M00);

    clkx->hsecs =
	    (VALIDBITS(clockbase->Reg1) * 10) + VALIDBITS(clockbase->Reg0);
    clkx->hmins =
	    (VALIDBITS(clockbase->Reg3) * 10) + VALIDBITS(clockbase->Reg2);
    clkx->hhours =
	    (VALIDBITS(clockbase->Reg5) * 10) + VALIDBITS(clockbase->Reg4);
    clkx->hdays =
	    (VALIDBITS(clockbase->Reg8) * 10) + VALIDBITS(clockbase->Reg7);
    clkx->hmonth =
	    (VALIDBITS(clockbase->RegA) * 10) + VALIDBITS(clockbase->Reg9);
    clkx->hyear =
	    (VALIDBITS(clockbase->RegC) * 10) + VALIDBITS(clockbase->RegB);
    clkx->hweekday = VALIDBITS(clockbase->Reg6) + 1;

    clockbase->RegD = (Ricoh_TIMER_EN|Ricoh_M01);
}

static int
clock_read(clkx)
struct ben *clkx;
{
    switch ((int)clock_type)
    {
    default:			/* our default clock for now */
    case RICOH:
	ricoh_read(clkx);
	break;

    case OKI:
	oki_read(clkx);
	break;
    }

    return check_clock(clkx);
}

static struct ben *
time2ben(clock)
time_t clock;
{
    register int d1;
    long hms, day;
    static struct ben clkx;

    /* break time into days */

    day = clock / SECONDS_PER_DAY; /* Number of days */
    hms = clock % SECONDS_PER_DAY; /* Hours left for this day (in seconds) */

    /* generate hours:minutes:seconds */

    d1 = hms / 60;		/* hours left for this day (in minutes) */
    clkx.hsecs = hms % 60;	/* seconds in this day */

    clkx.hhours = d1 / 60;	/* hours in this day */
    clkx.hmins = d1 % 60;	/* minutes in this day */

    /*
    ** day is the day number.
    ** generate day of the week.
    ** The addend is 4 mod 7 (1/1/1970 was Thursday)
    */

    clkx.hweekday = ((day + 7340036L) % 7) + 1;

    /* year number */

    for (clkx.hyear=70; day >= dysize(clkx.hyear); clkx.hyear++)
	day -= dysize(clkx.hyear);

    clkx.hdays = day;			/* days left in this year */

    /* generate month */

    if (dysize(clkx.hyear) == 366)	/* Leap year? */
	dmsize[1] = 29;			/*   then February has a leap day. */

    for (clkx.hmonth=0; clkx.hdays >= dmsize[clkx.hmonth]; ++clkx.hmonth)
	clkx.hdays -= dmsize[clkx.hmonth];
    ++clkx.hmonth;
    ++clkx.hdays;
#ifdef DEBUG
printf("time2ben: year=%d, month=%d, day=%d\n", clkx.hyear, clkx.hmonth, clkx.hdays);
#endif /* DEBUG */

    dmsize[1] = 28;

    return &clkx;
}

static time_t
ben2time(clkx)
struct ben *clkx;
{
    register time_t thetime = 0;
    int year;
    register int i;

    /* Add year */

    if (clkx->hyear < 70)
	year = clkx->hyear + 100;		/* ooooooohhhhh */
    else
	year = clkx->hyear;

    for (i=70; i < year; ++i)
	thetime += dysize(i) * SECONDS_PER_DAY;

    /* Add in month */
    if (dysize(year) == 366)
	dmsize[1] = 29;

    while (--clkx->hmonth)
	thetime += dmsize[clkx->hmonth-1] * SECONDS_PER_DAY;

    dmsize[1] = 28;

    /* Add in days */
    thetime += (clkx->hdays-1) * SECONDS_PER_DAY;

    /* Add in hours */
    thetime += clkx->hhours * SECONDS_PER_HOUR;

    /* Add in minutes */
    thetime += clkx->hmins * SECONDS_PER_MINUTE;

    /* Add in seconds */
    thetime += clkx->hsecs;

    return thetime;
}

/*
** get_clock_type: Returns the type of clock chip that is being used.
** Our test is basically to check if register f is 4 then the chip is
** a non-trashed Oki.  If register f is 0 then the chip is a Ricoh.
** then we do really weird stuff.
*/
static enum CLOCK_TYPE
get_clock_type()
{
    if (VALIDBITS(clockbase->RegF) == 4)
	return OKI;		/* Register F on Oki should always be 4 */

    clockbase->RegF = 0;		/* Reset controller */
    clockbase->RegE = 0;		/* Reset test modes */

    clockbase->RegD = (Ricoh_TIMER_EN|Ricoh_M01); /* Ricoh--Page 1 */
    clockbase->RegC = 5;	/* Random value == 1001 */
				/* RegC on rico should always be zero */
    if (VALIDBITS(clockbase->RegC) == 0)
    {
	/* It is a Ricoh or it isn't there. */
	if (VALIDBITS(clockbase->RegD) != (Ricoh_TIMER_EN|Ricoh_M01))
	    return NO_CLOCK;

	if (VALIDBITS(clockbase->RegA) == 0)
	    ricoh_reset();

	return RICOH;
    }

    /* It's either a trashed Oki or it isn't there. */

    clockbase->RegD = Oki_IRQ;
    clockbase->RegF = (Oki_2112|Oki_STOP|Oki_REST); /* reset */
    clockbase->RegF &= ~(Oki_STOP|Oki_REST); /* restart */

    clockbase->RegF = Oki_REST; /* reset */
    clockbase->RegF |= Oki_2112; /* set 24 bit */
    clockbase->RegF &= ~Oki_REST; /* restart */

    if (VALIDBITS(clockbase->RegF) != Oki_2112)
	return NO_CLOCK;

    oki_reset();			/* Trashed Oki... Resetting */

    return OKI;
}

int
benopen(devp, mode, type, cr)
dev_t *devp;
int mode;
int type;
struct cred *cr;
{
    if (clock_type == NO_CLOCK)
    {
	clock_type = get_clock_type();

	if (clock_type == NO_CLOCK)
	    return ENXIO;
    }

    return 0;
}

int
benread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
    time_t thetime;
    struct ben clkx;
    unsigned count;

    if (uiop->uio_offset < 0)
	return EINVAL;
    if (uiop->uio_resid == 0 || uiop->uio_offset >= sizeof(thetime))
	return 0;

    count = uiop->uio_resid;
    if (count > sizeof(thetime) - uiop->uio_offset)
	count = sizeof(thetime) - uiop->uio_offset;

    if (!clock_read(&clkx))
	return EIO;

    thetime = ben2time(&clkx);

    if (uiomove((caddr_t)&thetime + uiop->uio_offset, count, UIO_READ, uiop))
	return EFAULT;

    return 0;
}

int
benwrite(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
    time_t thetime;
    struct ben *clkx;

    if (uiop->uio_resid != sizeof(thetime))
	return EINVAL;

    if (uiomove(&thetime, sizeof(thetime), UIO_WRITE, uiop))
	return EFAULT;

    clkx = time2ben(thetime);

    clock_write(clkx);

#ifdef DEBUG
    printf("ClockWrite: 0x%x; %d %d %d %d %d [%d]\n", thetime,
	   clkx->hmonth, clkx->hdays, clkx->hhours, clkx->hmins, clkx->hyear,
	   clkx->hweekday);
#endif

    return 0;
}
