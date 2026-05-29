/*
 * Hardware information about the AMIGA.
 */

#include "audiohw.h"

/*
 * The main banks of custom chip registers: AMIGA, ACIAA and ACIAB.
 */
#define	AMIGA	((volatile struct amiga *)0xDFF000)
#define	ACIAA	((volatile struct acia *) 0xBFE001)
#define	ACIAB	((volatile struct acia *) 0xBFD000)


/*
 * The mess of registers created by angela, dorothy and pam or whatever
 * their names are currently.
 */
struct amiga {
	unsigned short		bltddat;
	unsigned short		dmaconr;
	unsigned short		vposr;
	unsigned short		vhposr;
	unsigned short		dskdatr;
	unsigned short		joy0dat;
	unsigned short		joy1dat;
	unsigned short		clxdat;
	unsigned short		adkconr;
	unsigned short		pot0dat;
	unsigned short		pot1dat;
	unsigned short		potinp;
	unsigned short		serdatr;
	unsigned short		dskbytr;
	unsigned short		intenar;
	unsigned short		intreqr;
	unsigned long		dskpt;
	unsigned short		dsklen;
	unsigned short		dskdat;
	unsigned short		refptr;
	unsigned short		vposw;
	unsigned short		vhposw;
	unsigned short		copcon;
	unsigned short		serdat;
	unsigned short		serper;
	unsigned short		potgo;
	unsigned short		joytest;
	unsigned short		strequ;
	unsigned short		strvbl;
	unsigned short		strhor;
	unsigned short		strlong;
	unsigned short		bltcon0;
	unsigned short		bltcon1;
	unsigned short		bltafwm;
	unsigned short		bltalwm;
	long			bltcpt;
	long			bltbpt;
	long			bltapt;
	long			bltdpt;
	unsigned short		bltsize;
	unsigned short		bltcon0l;	/* ECS */
	unsigned short		bltsizv;	/* ECS */
	unsigned short		bltsizh;	/* ECS */
	unsigned short		bltcmod;
	unsigned short		bltbmod;
	unsigned short		bltamod;
	unsigned short		bltdmod;
	unsigned short		pad34[4];
	unsigned short		bltcdat;
	unsigned short		bltbdat;
	unsigned short		bltadat;
	unsigned short		pad3b;
	unsigned short		sprhdat;	/* ECS */
	unsigned short		pad3d;
	unsigned short		deniseid;	/* ECS */
	unsigned short		dsksync;
	unsigned long		cop1lc;
	unsigned long		cop2lc;
	unsigned short		copjmp1;
	unsigned short		copjmp2;
	unsigned short		copins;
	unsigned short		diwstrt;
	unsigned short		diwstop;
	unsigned short		ddfstrt;
	unsigned short		ddfstop;
	unsigned short		dmacon;
	unsigned short		clxcon;
	unsigned short		intena;
	unsigned short		intreq;
	unsigned short		adkcon;
	struct amiga_audio	audio[NAUDIO];
	long			bplpt[6];
	unsigned short		pad7c[4];
	unsigned short		bplcon0;
	unsigned short		bplcon1;
	unsigned short		bplcon2;
	unsigned short		bplcon3;	/* ECS */
	unsigned short		bpl1mod;
	unsigned short		bpl2mod;
	unsigned short		pad86[2];
	unsigned short		bpldat[6];
	unsigned short		pad8e[2];
	long			sprpt[8];
	struct {
		unsigned short	sprpos;
		unsigned short	sprctl;
		unsigned short	sprdata;
		unsigned short	sprdatb;
	}			sprcon[8];
	unsigned short		color[32];
	unsigned short		htotal;		/* ECS */
	unsigned short		hsstop;		/* ECS */
	unsigned short		hbstrt;		/* ECS */
	unsigned short		hbstop;		/* ECS */
	unsigned short		vtotal;		/* ECS */
	unsigned short		vsstop;		/* ECS */
	unsigned short		vbstrt;		/* ECS */
	unsigned short		vbstop;		/* ECS */
	unsigned short		sprhstrt;	/* ECS */
	unsigned short		sprhstop;	/* ECS */
	unsigned short		bplhstrt;	/* ECS */
	unsigned short		bplhstop;	/* ECS */
	unsigned short		hhposw;		/* ECS */
	unsigned short		hhposr;		/* ECS */
	unsigned short		beamcon0;	/* ECS */
	unsigned short		hsstrt;		/* ECS */
	unsigned short		vsstrt;		/* ECS */
	unsigned short		hcenter;	/* ECS */
	unsigned short		diwhigh;	/* ECS */
	unsigned short		bplhmod;	/* ECS */
	unsigned short		sprhpth;	/* ECS */
	unsigned short		sprhptl;	/* ECS */
	unsigned short		bplhpth;	/* ECS */
	unsigned short		bplhptl;	/* ECS */
};


/*
 * Bits in interrupt enable register AMIGA->{intena,intenar,intreq,intreqr}.
 */
#define	AINTSET	0x8000
#define	AINTCLR	0x0000
#define	AIEMAST	0x4000			/* Master interrupt enable */
#define	AIEEXTI	0x2000			/* External interrupt */
#define	AIEDSYN	0x1000			/* Disk sync register match */
#define	AIESRBF	0x0800			/* Serial receive buffer full */
#define	AIEAUD3	0x0400			/* Audio channel 3 block done */
#define	AIEAUD2	0x0200			/* Audio channel 2 block done */
#define	AIEAUD1	0x0100			/* Audio channel 1 block done */
#define	AIEAUD0	0x0080			/* Audio channel 0 block done */
#define	AIEBLIT	0x0040			/* Blitter done */
#define	AIEVSYN	0x0020			/* Vertical sync */
#define	AIECOPI	0x0010			/* Copper interrupt */
#define	AIEINT2	0x0008			/* Interrupt 2 */
#define	AIESOFT	0x0004			/* Software initiated interrupt */
#define	AIEDBLK	0x0002			/* Disk block done */
#define	AIESTBE	0x0001			/* Serial transmit buffer empty */

/*
 * Bits in audio/disk control register AMIGA->{adkcon,adkconr}.
 */
#define	ADKSET	0x8000
#define	ADKCLR	0x0000

/*
 * Bits in DMA control register AMIGA->{dmacon,dmaconr}.
 */
#define	DMASET		0x8000
#define	DMACLR		0x0000
#define DMABLTNASTY	0x0400
#define DMAMASTER	0x0200
#define DMABPL		0x0100
#define DMACOP		0x0080
#define DMABLT		0x0040
#define DMASPR		0x0020
#define DMADSK		0x0010
#define DMAAUD3		0x0008
#define DMAAUD2		0x0004
#define DMAAUD1		0x0002
#define DMAAUD0		0x0001


/*
 * 8520 CIA registers.
 */
struct acia {
	unsigned char	pra;		/* Peripheral data register A	*/
	unsigned char	cia0[0xFF];
	unsigned char	prb;		/* Peripheral data register B	*/
	unsigned char	cia1[0xFF];
	unsigned char	ddra;		/* Data direction register A	*/
	unsigned char	cia2[0xFF];
	unsigned char	ddrb;		/* Data direction register B	*/
	unsigned char	cia3[0xFF];
	unsigned char	talo;		/* Timer A low register		*/
	unsigned char	cia4[0xFF];
	unsigned char	tahi;		/* Timer A high register	*/
	unsigned char	cia5[0xFF];
	unsigned char	tblo;		/* Timer B low register		*/
	unsigned char	cia6[0xFF];
	unsigned char	tbhi;		/* Timer B high register	*/
	unsigned char	cia7[0xFF];
	unsigned char	todl;		/* Time of day low register	*/
	unsigned char	cia8[0xFF];
	unsigned char	todm;		/* Time of day middle register	*/
	unsigned char	cia9[0xFF];
	unsigned char	todh;		/* Time of day high register	*/
	unsigned char	cia10[0xFF];
	unsigned char	cia11[0x100];
	unsigned char	sdr;		/* Serial data register		*/
	unsigned char	cia12[0xFF];
	unsigned char	icr;		/* Interrupt control register	*/
	unsigned char	cia13[0xFF];
	unsigned char	cra;		/* Control register A		*/
	unsigned char	cia14[0xFF];
	unsigned char	crb;		/* Control register B		*/
	unsigned char	cia15[0xFF];
};


/* Bits for the "icr" of the 8520 */
#define	ICR_SET		0x80		/* Set these bits */
#define ICR_CLR		0x00		/* Clear these bits */
#define ICR_TA		0x01		/* The "Timer A" interrupt bit */
#define ICR_TB		0x02		/* The "Timer B" interrupt bit */
#define ICR_SP		0x08		/* The serial port interrupt bit */
#define ICR_FLG		0x10		/* The "Flag" interrupt bit */
