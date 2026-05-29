/*
** AMIGA->serper
*/
#define	DATA8	0x0000

/*
** AMIGA->serdatr
*/
#define	RBF	0x4000
#define	TBE	0x2000

/*
** ACIAB->ddra (1 means output)
** ACIAB->pra (active low)
*/
#define	DTR	(1<<7 | 1<<6)
#define	RTS	(1 << 6)
#define	CD	(1 << 5)
#define	CTS	(1 << 4)

typedef enum
{
    OPEN,
    CLOSE,
    SETBREAK,
} xmitter;

typedef struct
{
    int			inputglobsize;
    int			everysooften;
    int 		carrier;
    int			cts;
    int			output_xoff;
    struct queue	*rdq;
    struct strtty	tty;
} sl_t;

#define blklen(bp)	(bp->b_wptr - bp->b_rptr)

#define input_mblk	tty.t_in.bu_bp
#define output_mblk	tty.t_out.bu_bp

#define ienable(bits)   AMIGA->intena = AINTSET | bits;
#define idisable(bits)  AMIGA->intena = AINTCLR | bits;
#define irequest(bits)  AMIGA->intreq = AINTSET | bits; ienable(bits);

#define hardware_flow_control	(!sl.tty.t_dev)

#define EVERYSOOFTEN	(HZ/HZ)
#define INPUTGLOBSIZE	(128)

#define FALSE (0)
#define TRUE (1)
