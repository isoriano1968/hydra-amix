#ifndef _AMIGA_HYRAUSER_H
#define _AMIGA_HYRAUSER_H

#define HYDRAIOC		('H'<<8)
#define HYDRA_CLEAR_STATUS	(HYDRAIOC|1)
#define HYDRA_GET_STATUS	(HYDRAIOC|2)
#define HYDRA_GET_CONFIG	(HYDRAIOC|3)
#define HYDRA_SET_CONFIG	(HYDRAIOC|4)
#define HYDRA_NUMBER_OF_BOARDS	(HYDRAIOC|5)

#define HYDRA_RAW_DATA		(1)

enum HYDRA_BOARD_STATE
{
    HYDRA_BOARD_RESET,
    HYDRA_BOARD_RUNNING
};

typedef struct
{
    enum HYDRA_BOARD_STATE	board_state;
    unsigned int		packets_sent;
    unsigned int		packets_received;
    unsigned int		allocbs_failed;
    unsigned int		couldnt_put;
    unsigned int		tx_errors;
    unsigned int		rx_errors;
    unsigned int		collisions;
    unsigned int		overflow;
    unsigned int		crc_errors;
    unsigned int		framing_errors;
    unsigned int		missed_packets;
    unsigned int		buffer_error;
    unsigned int		trailers;
    unsigned int		board_debug;
    unsigned int		late_collisions;
    unsigned int		loss_of_carrier;
    unsigned int		retry_errors;
} hydra_status_t;

typedef struct
{
    long			board_base;
    int				board_debug;
    unsigned char		paddress[6];
    unsigned char		laddress[8];
    int				mode;
    int				flags;
} hydra_config_t;

#endif
