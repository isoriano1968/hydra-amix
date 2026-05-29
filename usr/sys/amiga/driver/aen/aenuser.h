#ifndef _AMIGA_AENUSER_H
#define _AMIGA_AENUSER_H

#define	AENIOC			('A'<<8)
#define AEN_CLEAR_STATUS	(AENIOC|1)
#define AEN_GET_STATUS		(AENIOC|2)
#define AEN_GET_CONFIG		(AENIOC|3)
#define AEN_SET_CONFIG		(AENIOC|4)
#define AEN_NUMBER_OF_BOARDS	(AENIOC|5)

/*
** Initialization Block - Mode Register
*/

#define AEN_MODE_DRX  0x0001	/* Disable The Receiver */
#define AEN_MODE_DTX  0x0002	/* Disable The Transmitter */
#define AEN_MODE_LOOP 0x0004	/* Loopback */
#define AEN_MODE_DTCR 0x0008	/* Disable Transmit CRC */
#define AEN_MODE_COLL 0x0010	/* Force Collision */
#define AEN_MODE_DRTY 0x0020	/* Disable Retry */
#define AEN_MODE_INTL 0x0040	/* Internal Loopback */
#define AEN_MODE_PROM 0x8000	/* Promiscuous Mode */

/*
** flags
*/
#define AEN_RAW_DATA		(1)

enum BOARD_STATE
{
    BOARD_RESET,
    BOARD_IN_INIT_CODE,
    BOARD_RUNNING
};

typedef struct
{
    enum BOARD_STATE		board_state;
    unsigned int		packets_sent;
    unsigned int		packets_received;
    unsigned int		allocbs_failed;
    unsigned int		couldnt_put;
    unsigned int		memory_error;
    unsigned int		miss_error;
    unsigned int		collision_error;
    unsigned int		babble_error;
    unsigned int		trailers;
    unsigned int		bad_start;
    unsigned int		buffer_error;
    unsigned int		overflow;
    unsigned int		framming;
    unsigned int		crc;
    unsigned int		board_debug;
    unsigned int		late_collisions;
    unsigned int		loss_of_carrier;
    unsigned int		retry_errors;
} aen_status_t;

typedef struct
{
    long			board_base;	 /* Board config location */
    int				board_debug;
    unsigned char		paddress[6];	 /* Physical address */
    unsigned char		laddress[8];	 /* Logical address filter */
    int				mode;		 /* permiscuious mode? */
    int				flags;		 /* Software modes */
} aen_config_t;

#endif /* _AMIGA_AENUSER_H */
