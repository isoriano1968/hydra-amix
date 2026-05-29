/*
 * Auto configuration name.
 */
#define	APNDISK	0x02020001		/* Disk controller */

/*
 * Bits in registers.
 */
#define	BICOM	0x10			/* (Interrupt) command complete */
#define	BIPEN	0x80			/* (Interrupt) interrupt pending */

#define	BCIEN	0x10			/* (Control) interrupt enable */
#define	BCSEL	0x20			/* (Control) select */
#define	BCRES	0x40			/* (Control) reset */
#define	BCCBP	0x80			/* (Control) command block pointer */


/*
 * Data for registers.
 */
#define	BDACK	0x0000			/* (Data) acknowledge interrupt */
#define	BSACK	0x0000			/* (Start) acknowledge interrupt */
#define	BSTAR	0x0001			/* (Start) start command */


/*
 * ST506 commands.
 */
#define	CSDPM	0x0C			/* Set drive parameters */
#define	CRCOM	0x08			/* Read */
#define	CWCOM	0x0A			/* Write */
#define	CMOVE	0x0F			/* Move command block */


/*
 * SCSI registers.
 */
#define	RIDEN	0x00			/* ID */
#define	RCONT	0x01			/* Control */
#define	RTIME	0x02			/* Timeout */
#define	RCDBA	0x03			/* Command descriptor block */
#define	RTARG	0x0F			/* Target */
#define	RCOMP	0x10			/* Command phase */
#define	RTCNT	0x12			/* Transfer count */
#define	RUNIT	0x15			/* Destination identifier */
#define	RSTAT	0x17			/* Status */
#define	RCOMM	0x18			/* Command */
#define	RDATA	0x19			/* Data register */
#define	RAUXS	0x1F			/* Auxiliary status */


/*
 * SCSI commands.
 */
#define	SRCOM	0x28			/* Read */
#define	SWCOM	0x2A			/* Write */


/*
 * Device registers.
 */
struct hdreg {
	unsigned char	r_fil0[64];	/* Miscellaneous stuff */
	unsigned char	r_intr;		/* Interrupt register */
	unsigned char	r_fil1;		/* Filler */
	unsigned char	r_cont;		/* Control register */
	unsigned char	r_fil2[5];	/* Filler */
	unsigned char	r_base;		/* Base register */
	unsigned char	r_fil3[7];	/* Filler */
	unsigned short	r_data;		/* Data register */
	unsigned short	r_star;		/* Start command */
	unsigned char	r_fil4[12];	/* Filler */
	unsigned char	r_scsa;		/* SCSI address */
	unsigned char	r_fil5;		/* Filler */
	unsigned char	r_scsd;		/* SCSI data */
	unsigned char	r_fil6;		/* Filler */
	unsigned char	r_dmaa;		/* DMA address */
	unsigned char	r_fil7[3];	/* Filler */
	unsigned char	r_dmad;		/* DMA data */
};


/*
 * Structure of ST506 command block in memory.
 */
struct hdcom {
	unsigned char	c_code;		/* Opcode */
	unsigned char	c_usec;		/* Unit and high sector address */
	unsigned short	c_lsec;		/* Low sector address */
	unsigned char	c_nblk;		/* Number of sectors */
	unsigned char	c_res1;		/* Reserved */
	unsigned char	c_hdma;		/* High dma address */
	unsigned char	c_mdma;		/* Middle dma address */
	unsigned char	c_ldma;		/* Low dma address */
	unsigned char	c_res2;		/* Reserved */
	unsigned char	c_res3;		/* Reserved */
	unsigned char	c_res4;		/* Reserved */
	unsigned char	c_errs;		/* Error information */
	unsigned char	c_herr;		/* High error information */
	unsigned short	c_lerr;		/* Low error information */
};


/*
 * Parameters to initialise ST506 drive.
 */
struct hdpar {
	unsigned char	p_opst;		/* Options, step */
	unsigned char	p_hdch;		/* Head, cylinder (high order) */
	unsigned char	p_cyll;		/* Low order bits of cylinder */
	unsigned char	p_pcyl;		/* Precomputed cylinder/16 */
	unsigned char	p_redw;		/* Reduced write current */
	unsigned char	p_nsec;		/* Number of sectors per track */
};
