
#define RESOLVER_ID	0x08510001
#define HWLEN		0x00010000

#ifndef GSP_REG_DEFINED
#define GSP_REG_DEFINED
struct GSP_REG
{
    unsigned char pad0[0x90];
    union
    {
	struct 
	{
	    unsigned short hstadrl;	/* 0x90(short) */
	    unsigned short hstadrh;	/* 0x92(short) */
	} hstadrshorts;
	unsigned long hstadr;		/* 0x90(long) */
    } u;
    unsigned short hstdata;		/* 0x94(short) */
    unsigned short hstctl;		/* 0x96(short) */
    unsigned char pad1[0x28];
    unsigned short dotclk;		/* 0xC0(short) */
    unsigned char pad2[0x1e];
    unsigned short ovlen;		/* 0xE0(short) */
};

/* Host control register bits */
#define HLT	 0x8000      /* Halt GSP */
#define CF	 0x4000      /* cache flush */
#define LBL	 0x2000      /* lower byte last */
#define INCR	 0x1000      /* Increment on read cycles */
#define INCW	 0x0800      /* Increment on write cycles */
#define NMIMODE  0x0200      /* NMI mode (no stack usage) */
#define NMI	 0x0100      /* Non maskable int */
#define INTOUT	 0x0080      /* MSG Interrupt out */
#define MSGOUT	 0x0070      /* Message out */
#define INTIN	 0x0008      /* MSG Interrupt in */
#define MSGIN	 0x0007      /* Message in */

#define DOTCLOCK 0x03200000

#define B459AL 0x0000
#define B459AH 0x0010
#define B459IC 0x0020
#define B459IP 0x0030

#endif

#define splres spl2
