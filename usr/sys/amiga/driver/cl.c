/*
 * A clock driver using the Amiga vertical blank interrupt or an 8520 ACIA.
 * See also acia.c.
 */

#include	"sys/types.h"
#include	"sys/param.h"
#include	"sys/inline.h"
#include	"amigahr.h"
#include	"screen.h"

/* param.h has hibyte/lobyte that don't work */
#undef lobyte
#undef hibyte

#define lobyte(x) ((unsigned char)(x))
#define hibyte(x) ((unsigned char)((unsigned)(x)>>8))


/* CLOCK should be 0 (vblank), 1 (ACIAA), or 2 (ACIAB) */

#ifndef CLOCK
#define CLOCK 1
#endif

#if CLOCK == 0
#define splclk spl3
#define clkint AIEVSYN
#else
#if CLOCK == 1
#define ACIA ACIAA
#define splclk spl2
#define clkint AIEINT2
#else
#if CLOCK == 2
#define ACIA ACIAB
#define splclk spl6
#define clkint AIEEXTI
#else
error "Unknown CLOCK definition"
#endif
#endif
#endif

#ifdef ACIA
#define EHZ		(PAL?709379:715909)
#define ATICKS		((EHZ+HZ/2)/HZ)
#endif


/*
 * Stop the clock when the system has paniced.
 * If vblanks are used for the clock source, the clock can't be turned off.
 * More likely, an ACIA is used for the clock source, and we disable
 * its timer interrupt.
 */
void clkreld()
{
#ifdef ACIA
	ACIA->icr = ICR_CLR|ICR_TA; /* Disable timer interrupt */
	ACIA->cra = 0;
#endif
}


/*
 * This file is a strange place for the displaytype stuff to be,
 * but the clock implementation is intimately related to whether
 * the machine is PAL or NTSC, and this is determined by checking
 * the timing of VBLANKs.  Thus, all in this file for now.
 */

unsigned int displaytype=0;

/*
 * Figure out whether this machine is PAL or NTSC (50 or 60 HZ video)
 */
void figuredisplaytype(x)
{
	int ticksperfield, s;
#define PALTPF	(709379/50)
#define NTSCTPF	(715909/60)
	s=_spl7();
	while ( (AMIGA->vhposr & 0xff00) || (AMIGA->vposr & 0x1) )
		;
	ACIAA->cra = 8;
	ACIAA->talo = 0xff;
	ACIAA->tahi = 0xff;
	while ( !(AMIGA->vhposr & 0xff00) && !(AMIGA->vposr & 0x1) )
		;
	while ( (AMIGA->vhposr & 0xff00) || (AMIGA->vposr & 0x1) )
		;
	ACIAA->cra = 0;		/* Stop the counter */
	ticksperfield = 0xffff - ((ACIAA->tahi << 8) | ACIAA->talo);
	splx(s);
	if (ticksperfield > (PALTPF+NTSCTPF)/2)
		displaytype = 2;	/* 2 == PAL */
	else
		displaytype = 1;	/* 1 == NTSC */
#if 0
	printf("NTSCTPF=%d, PALTPF=%d\n",
	       NTSCTPF, PALTPF);
	printf("%d ticks per field ... displaytype %x\n",
	       ticksperfield, displaytype);
	printf("(called from 0x%x)\n", (long *)(&x)[-1]);
#endif

	displaytype |= (AMIGA->vposr & 0x7f00) >> (24-DTB_AGNUSVER);
	displaytype |= (AMIGA->deniseid & 0xff) << DTB_DENISEVER;
}


/*
 * Initialize the clock.
 */
void hw_clkstart()
{
#if CLOCK == 0
	if (!displaytype)
		figuredisplaytype();
#endif
	AMIGA->intena = AINTSET|clkint;
#ifdef ACIA
	/*printf("	HZ=%d, TICK=%d, EHZ=%d, ATICKS=0x%x\n",
	       HZ, TICK, EHZ, ATICKS);*/
	ACIA->cra = 0;
	ACIA->icr = ICR_SET|ICR_TA; /* Enable timer interrupt */
	ACIA->talo = lobyte(ATICKS);
	ACIA->tahi = hibyte(ATICKS);
	ACIA->cra = 0x11;	/* Start the counter */
#endif
}
