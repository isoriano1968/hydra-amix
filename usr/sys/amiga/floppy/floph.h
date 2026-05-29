/*
 * This include file describes the floppy disk hardware.
 */


/*
 * Bits in ACIAA->pra (all read only).
 * Note, all the names are really backwards because of the
 * silly `active low' phenomenon.
 */
#define	DSKRDY		(1 << 5)	/* disk not ready */
#define	DSKTRACK0	(1 << 4)	/* heads not at cylinder 0 */
#define	DSKPROT		(1 << 3)	/* disk is not write protected */


/*
 * Bits in ACIAB->prb.
 * Note, many of the names are really backwards because of the
 * silly `active low' phenomenon.
 */
#define	DSKMOTOR	(1 << 7)	/* do not turn on the drive motor */
#define	DSKSEL3		(1 << 6)	/* do not select drive 3 */
#define	DSKSEL2		(1 << 5)	/* do not select drive 2 */
#define	DSKSEL1		(1 << 4)	/* do not select drive 1 */
#define	DSKSEL0		(1 << 3)	/* do not select drive 0 */
#define	DSKSEL		(DSKSEL0 | DSKSEL1 | DSKSEL2 | DSKSEL3)
#define	DSKSIDE		(1 << 2)	/* select lower (vs. higher) head */
#define	DSKDIREC	(1 << 1)	/* step out (vs. in) */
#define	DSKSTEP		(1 << 0)	/* not step disk */


/*
 * Bits in ACIAB->icr (read only?).
 */
#define	DSKINDEX	(1 << 4)	/* disk not at index position */


/*
 * Bits in AMIGA->dsklen.
 */
#define	DMAEN		(1 << 15)	/* disk dma enable */
#define	WRITE		(1 << 14)	/* write to disk */


/*
 * Bits in AMIGA->adkcon.
 */
#define	PRECOMP1	(1 << 14)	/* msb of precomp specifier */
#define	PRECOMP0	(1 << 13)	/* lsb of precomp specifier */
#define	MFMPREC		(1 << 12)	/* use MFM (vs. GCR) precomp */
#define	WORDSYNC	(1 << 10)	/* disk read uses DSKSYNC */
#define	MSBSYNC		(1 << 9)	/* sync on msb (for GCR) */
#define	FAST		(1 << 8)	/* 2 (vs. 4) mics per bit */


/*
 * Delay times (in microseconds).
 */
#define	MOTOR_NAP_DEL	250000		/* after turning motor on */
#define	MOTOR_MAX_NAPS	    10		/* maximum number of naps */
#define	SETTLE_DEL	 15000		/* after seeking */
#define	STEP_DEL	  3000		/* after each step */
#define	POST_WRITE_DEL	  2000		/* after writing */
#define	SIDE_SELECT_DEL	  1000		/* after switching sides */


/*
 * Disk info. which is not dependant on any particular disk format.
 * Note, you shouldn't count on being able to get more than TRKUSED
 * bytes per track, but you might get (and hence must read and write)
 * as many as TRKUSED + TRKSLOP.
 */
#define	RECSLOP		20		/* extra steps for recalibrate */
#define	TRACKS		160		/* number of tracks per disk */
#define	TRKPCYL		2		/* tracks per cylinder */
#define	TRKUSED		11968		/* max. bytes to use per track */
#define	TRKSLOP		1660		/* possible extra bytes on track */
#define	PCOMP1		80		/* lowest track using 1 precomp. */
#define	PCOMP2		(TRACKS+1)	/* lowest track using 2 precomp. */
#define	PCOMP3		(TRACKS+1)	/* lowest track using 3 precomp. */
#define	MPERB		(2*bitsof(uchar))	/* mics per byte */
