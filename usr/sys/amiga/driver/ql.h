
#define	NCARD		3
#define	NLINE		7

#define	qlcard(dev)	((dev)>>3 & 07)
#define	qlline(dev)	((dev) & 07)

#define	BUFSIZE	0x0100

typedef struct
{
    unsigned char	rhead;
    unsigned char 	rcondition;
    unsigned char	rsync;
    unsigned char	tdisable;
    unsigned char	thead;
    unsigned char	ttail;
    unsigned char	ttandem;
    unsigned char	tflush;
    unsigned char	setparams;
    unsigned char	control;
    unsigned char	command;
    unsigned char	xon;
} line_t;

/*
** line.rcondition
*/
#define	BREAK_DETECTED	1
#define	CARR_DETECTED	2
#define	CARR_DROPPED	3
#define	RECEIVER_SYNC	4

typedef struct
{
    line_t		line[NLINE];
    unsigned char	xxx1[0x01AC];
    unsigned char	tbuf[NLINE][BUFSIZE];
    unsigned char	rbuf[NLINE][BUFSIZE];
    unsigned char	xxx2[0x3000];
    unsigned char	irqack;
    unsigned char	xxx3[0x7FFF];
    unsigned char	start;
} device_t;

typedef struct
{
    struct strtty	*tp;
    line_t		*lp;
    unsigned char	*tbufp;
    unsigned char	*rbufp;
    unsigned int	rtail;
    int			carrier;
    int			rsync;
    int			rts;
} ql_t;

typedef enum
{
    OPEN,
    CLOSE,
    SETBREAK,
    RESETRTS,
} xmitter;

#define FALSE (0)
#define TRUE (1)

#define blklen(bp)	(bp->b_wptr - bp->b_rptr)
#define	nel(a)		(sizeof((a)) / sizeof((a)[0]))
#define	endof(a)	((a) + nel(a))
