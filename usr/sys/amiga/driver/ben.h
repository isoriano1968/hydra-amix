#ifndef _AMIGA_BEN_H
#define _AMIGA_BEN_H

enum CLOCK_TYPE
{
    NO_CLOCK,			/* 0 = no or unkown clock in system */
    RICOH,			/* RicoH (tudor) clock [A3000] */
    OKI,			/* OkiData clock [A2500] */
};

#define CLOCKBASE	((clock_registers_t *)0xDC0000)

typedef struct clock_registers
{
    unsigned int  :24;
    unsigned char Reg0;
    unsigned int  :24;
    unsigned char Reg1;
    unsigned int  :24;
    unsigned char Reg2;
    unsigned int  :24;
    unsigned char Reg3;
    unsigned int  :24;
    unsigned char Reg4;
    unsigned int  :24;
    unsigned char Reg5;
    unsigned int  :24;
    unsigned char Reg6;
    unsigned int  :24;
    unsigned char Reg7;
    unsigned int  :24;
    unsigned char Reg8;
    unsigned int  :24;
    unsigned char Reg9;
    unsigned int  :24;
    unsigned char RegA;
    unsigned int  :24;
    unsigned char RegB;
    unsigned int  :24;
    unsigned char RegC;
    unsigned int  :24;
    unsigned char RegD;
    unsigned int  :24;
    unsigned char RegE;
    unsigned int  :24;
    unsigned char RegF;
} clock_registers_t;

#define VALIDBITS(x) ((x)&0xF)

/* Ricoh register D bit fields */

#define Ricoh_TIMER_EN		(8)
#define Ricoh_ALARM_EN		(4)
#define Ricoh_M1		(2)
#define Ricoh_M0		(1)
#define Ricoh_M00		(0)
#define Ricoh_M01		(Ricoh_M0)
#define Ricoh_M10		(Ricoh_M1)
#define Ricoh_M11		(Ricoh_M1|Ricoh_M0)

/* Ricoh register E bit fields */

#define Ricoh_TEST3		(8)
#define Ricoh_TEST2		(4)
#define Ricoh_TEST1		(2)
#define Ricoh_TEST0		(1)

/* Ricoh register F bit fields */

#define Ricoh_1HZ_ON		(8)
#define Ricoh_16HZ_ON		(4)
#define Ricoh_TIMER_RESET	(2)
#define Ricoh_ALARM_RESET	(1)

/* Oki register D bit fields */

#define Oki_30SEC_ADJ		(8)
#define Oki_IRQ			(4)
#define Oki_BUSY		(2)
#define Oki_HOLD		(1)

/* Oki register E bit fields */

#define Oki_T1			(8)
#define Oki_T0			(4)
#define Oki_ITRPT		(2)
#define Oki_MASK		(1)

/* Oki register F bit fields */

#define Oki_TEST		(8)
#define Oki_2112		(4) /* BIFF WUZ HERE! RAGE ON! */
#define Oki_STOP		(2)
#define Oki_REST		(1)

struct ben
{
    short hsecs;
    short hmins;
    short hhours;
    short hdays;
    short hweekday;
    short hmonth;
    short hyear;
};

#endif /* _AMIGA_BEN_H */
