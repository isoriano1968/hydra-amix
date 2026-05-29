/*
 * Variables passed in memory from the A2620/A2630 ROM to Unix.
 */

#define	AV	((struct   avar *)0x078000)

/* Configuration structure. */
struct acon {
	unsigned long	ac_pnum;	/* Product identifier */
	unsigned long	ac_base;	/* Base register */
	unsigned long	ac_over;	/* Address space used */
	unsigned long	ac_fill;	/* Reserved */
};

#define	NAUTO	16			/* Number of configuration slots */
struct avar {
	unsigned char	av_pad[128];
	struct acon	av_acon[NAUTO];	/* 080[100]: Configuration information */
};

/* Special autoconfiguration names passed by the ROM. */
#define APNARAM	0x01000000		/* Amiga random access memory */
#define	APNMEMB	0x0202000A		/* Memory board */
#define	APNM020	0x02020050		/* 68020 board */
#define	APNM030	0x02020051		/* 68030 board */
