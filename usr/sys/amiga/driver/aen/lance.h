#ifndef _AMIGA_LANCE_H
#define _AMIGA_LANCE_H

/*
** The following definitions are constants used in bit-manipulation of
** the LANCE registers.
*/

/*
** Control Register Zero (0)
*/

#define CSR0_INIT 0x0001	/* Initialize LANCE */
#define CSR0_STRT 0x0002	/* Start LANCE */
#define CSR0_STOP 0x0004	/* Stop LANCE */
#define CSR0_TDMD 0x0008	/* Transmit Demand */
#define CSR0_TXON 0x0010	/* Transmitter On */
#define CSR0_RXON 0x0020	/* Receiver On */
#define CSR0_INEA 0x0040	/* Enable LANCE Interrupts */
#define CSR0_INTR 0x0080	/* LANCE Interrupt Line */
#define CSR0_IDON 0x0100	/* Initialization Done */
#define CSR0_TINT 0x0200	/* Transmitter Interrupt */
#define CSR0_RINT 0x0400	/* Receiver Interrupt */
#define CSR0_MERR 0x0800	/* Memory Error */
#define CSR0_MISS 0x1000	/* Missed Packet */
#define CSR0_CERR 0x2000	/* Collision Error */
#define CSR0_BABL 0x4000	/* Babble Error */
#define CSR0_ERR  0x8000	/* Error Summary */

/*
** Control Register Three (3)
*/

#define CSR3_BCON 0x0001	/* Byte Control */
#define CSR3_ACON 0x0002	/* ALE Control */
#define CSR3_BSWP 0x0004	/* BYTE Swap */

/*
** Register Address Port
*/

#define RAP_CSR0  0x0000	/* Register Address - CSR 0 */
#define RAP_CSR1  0x0001	/* Register Address - CSR 1 */
#define RAP_CSR2  0x0002	/* Register Address - CSR 2 */
#define RAP_CSR3  0x0003	/* Register Address - CSR 3 */

/*
** Initialization Block - Mode Register
*/

#define MODE_DRX  0x0001	/* Disable The Receiver */
#define MODE_DTX  0x0002	/* Disable The Transmitter */
#define MODE_LOOP 0x0004	/* Loopback */
#define MODE_DTCR 0x0008	/* Disable Transmit CRC */
#define MODE_COLL 0x0010	/* Force Collision */
#define MODE_DRTY 0x0020	/* Disable Retry */
#define MODE_INTL 0x0040	/* Internal Loopback */
#define MODE_PROM 0x8000	/* Promiscuous Mode */

/*
** Receive Message Descriptor One (1)
*/

#define RMD1_ENP  0x01	/* End Of Packet */
#define RMD1_STP  0x02	/* Start Of Packet */
#define RMD1_BUFF 0x04	/* Buffer Error */
#define RMD1_CRC  0x08	/* CRC Error */
#define RMD1_OFLO 0x10	/* Overflow Error */
#define RMD1_FRAM 0x20	/* Framing Error */
#define RMD1_ERR  0x40	/* Error Summary */
#define RMD1_OWN  0x80	/* Descriptor Entry Ownership */

/*
** Transmit Message Descriptor One (1)
*/

#define TMD1_ENP  0x01	/* End Of Packet */
#define TMD1_STP  0x02	/* Start Of Packet */
#define TMD1_DEF  0x04	/* Deferred/Busy */
#define TMD1_ONE  0x08	/* One Retry */
#define TMD1_MORE 0x10	/* More Than One Retry */
#define TMD1_ERR  0x40	/* Error Summary */
#define TMD1_OWN  0x80	/* Descriptor Entry Ownership */

/*
** Transmit Message Descriptor Three (3)
*/

#define TMD3_TDR  0x03FF	/* Time Domain Reflectometry Mask */
#define TMD3_RTRY 0x0400	/* Retry Error */
#define TMD3_LCAR 0x0800	/* Loss Of Carrier */
#define TMD3_LCOL 0x1000	/* Late Collision */
#define TMD3_UFLO 0x4000	/* Underflow Error */
#define TMD3_BUFF 0x8000	/* Buffer Error */

/*
** Common Message Descriptor Definitions
*/

#define MD1_ENP	  0x01	/* Set/Show ENP Bit */
#define MD1_STP	  0x02	/* Set/Show STP Bit */
#define MD1_ERR	  0x40	/* Error Summary */
#define MD1_OWN	  0x80	/* Give LANCE Ownership */

/*
** Descriptor Ring Lengths
*/

#define RLEN_1    0		/*   1 Entry */
#define RLEN_2    1		/*   2 Entries */
#define RLEN_4    2		/*   4 Entries */
#define RLEN_8    3		/*   8 Entries */
#define RLEN_16   4		/*  16 Entries */
#define RLEN_32   5		/*  32 Entries */
#define RLEN_64   6		/*  64 Entries */
#define RLEN_128  7		/* 128 Entries */

/*
** The following definitions are the default values used to initialize
** the LANCE chip and it's Descriptor Rings.
*/

#define TXRLEN	  RLEN_8	/* Default TX Ring Length */
#define RXRLEN	  RLEN_128	/* Default RX Ring Length */
#define MODE	  0x0000	/* Default Mode Register */

/*
** The following is the structure definition for a LANCE
** Initialization Block.
*/

struct initblk
{
    unsigned short	mode;		/* Mode Register */
    unsigned char	padr[6];	/* Physical Address */
    unsigned char	ladrf[8];	/* Logical Address Filter */
    unsigned short	rxring[2];	/* Receive Descriptor Ring Pointer */
    unsigned short	txring[2];	/* Transmit Descriptor Ring Pointer */
};

/*
** The following are structure definitions for Descriptor Rings.
*/

struct ringptr
{
    unsigned short	loaddr;		/* lo word address for buffer */
    unsigned char	mode;		/* mode address */
    unsigned char	hiaddr;		/* high order address */
    short		length;		/* (negated) length of buffer */
    unsigned short	count;		/* read byte count */
};

#endif /* _AMIGA_LANCE_H */
