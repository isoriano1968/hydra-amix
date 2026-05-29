#ifndef _AMIGA_AEN_H
#define _AMIGA_AEN_H

#define AEN_MAXDEV	5
#define AEN_MAXBOARDS	4

/*
** Ethernet address - 6 octets
*/
typedef u_char ether_addr_t[6];

/*
** Structure of a 10Mb/s Ethernet header.
*/
struct	ether_header
{
    ether_addr_t ether_dhost;
    ether_addr_t ether_shost;
    u_short	ether_type;
};

typedef struct ether_header ether_header_t;

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

/*
** The Commodore Ethernet Board has 32k of RAM accessable by
** The lance chip.
*/ 
#define RX_BUFFER_LENGTH	(150)	/* size of each lance RX buffer */
#define TX_BUFFER_LENGTH	(1518)	/* size of each lance TX buffer */
#define MAX_BUFFER_LENGTH	(1518)	/* max ethernet packet len */
#define AEN_MTU (MAX_BUFFER_LENGTH-sizeof(ether_header_t)-ETH_CRC_LEN)
					/* maximum xfer unit */
#define AEN_MIN		(64-ETH_CRC_LEN)		/* minimum pkt size */
#define AEN_MAX		AEN_MTU			/* maximum pkt size */
#define AEN_LOWAT	(2*AEN_HIWAT/3)		/* low water mark */
#define AEN_HIWAT	(1024*5)		/* high water mark */

#define ETH_ADDRESS_SIZE (6)
#define ETH_TYPE_SIZE	 (2)
#define ETH_LENGTH_SIZE	 (2)
#define ETH_CRC_LEN	 (4)			/* Length of CRC */
#define ETH_HEADER_SIZE	 (ETH_ADDRESS_SIZE+ETH_ADDRESS_SIZE+ETH_TYPE_SIZE)
#define ETH_MAXDATA	 (1500)
#define ETH_MAXPACKET	 (ETH_HEADER_SIZE+ETH_MAXDATA)
#define ETH_MINDATA	 (50)
#define ETH_MINPACKET	 (ETH_HEADER_SIZE+ETH_MINDATA)

#undef TRUE
#undef FALSE
#define TRUE	(0==0)
#define FALSE	(!TRUE)

#define AEN_INIT_PRI	(PZERO-1)		/* heavy sleeper during init */

#define	RXCOUNT	(1<<RXRLEN)
#define	TXCOUNT	(1<<TXRLEN)

#define AMBOARD   0x041d0001	/* manufact num/prod code for AmerStar board */
#define AEN_BOARD 0x02020070	/* manufact num/prod code for CBM board */

#define splaen	spl2
#define blklen(bp)	(bp->b_wptr - bp->b_rptr)

typedef struct
{
    queue_t		*q;		/* queue pointer */
    int			state;
    int			board_index;
    int			flags;
    unsigned short	sap;		/* protocol type of this stream */
} aen_t;

typedef struct
{
    long		board_base;		/* Board config location */
    volatile unsigned short	*lance_base;
    volatile unsigned short	*lance_data;
    volatile unsigned short	*lance_addr;
    unsigned char	paddress[6];	       	/* Physical address */
    unsigned char	laddress[8];		/* Logical address filter */
    int			tx_buffer_index;
    int			rx_buffer_index;	/* indexes into ring buffers */
    volatile struct initblk	*initblk;
    volatile struct ringptr	*rxptr;
    volatile struct ringptr	*txptr;
    void		*lance_buff;
} aen_info_t;

typedef struct
{
    aen_t		aen[AEN_MAXDEV];
    aen_status_t	aen_status;
    aen_info_t		aen_info;
    struct ifstats	ifstats;
} aen_board_t;

typedef struct
{
    long		address;
    int			type;
} aen_autoconfig_t;

/*#define DEBUG0			/* debugging on? */

/*#define DEBUG  YES			/* ASSERT()s */

#ifdef DEBUG0
#define DEBUG1	0x000000001		/* dump sending packets */
#define DEBUG2	0x000000002		/* dump recevied packets */

#define DEFAULT_DEBUG	0

#define BUGGYBUTTON  (!(AMIGA->potinp & 1<<10) ? board->aen_status.board_debug : 0)
#endif /* DEBUG0 */

#endif /* _AMIGA_AEN_H */
