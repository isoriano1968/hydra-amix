/*
** Hydra EtherNet driver - Hydra Systems AmigaNet (DP8390/NE2000)
**
** Zorro II Ethernet card using National Semiconductor DP8390 NIC.
** Revision 1.2a supported: 10Base2 (BNC) and 10BaseT (RJ45).
*/

#include "sys/types.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/errno.h"
#include "sys/psw.h"
#include "sys/pcb.h"
#include "sys/user.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/strlog.h"
#include "sys/log.h"
#include "sys/queue.h"
#include "sys/cio_defs.h"
#include "sys/sbd.h"
#include "sys/debug.h"
#include "sys/inline.h"
#include "sys/systm.h"
#include "sys/cred.h"
#include "sys/dlpi.h"
#include "sys/ddi.h"
#include "sys/cmn_err.h"
#include "sys/socket.h"
#include "sys/sockio.h"
#include "net/if.h"
#include "hydrauser.h"
#include "hydra.h"
#include "ne2000.h"
#include "../kdb/kdebug.h"

extern struct ifstats *ifstats;

extern char *panicstr;
void hydraautoconfig();
void hydrainit();

/* AutoConfig ROM nibble decoding constants */
#define AC_TYPE    0
#define AC_PRODUCT 1
#define AC_FLAGS   2
#define AC_RESV    3
#define AC_MANUF   4
#define AC_SERIAL  6
#define AC_DIAGVEC 10

#define ERT_TYPEMASK  0xC0
#define ERT_ZORROII   0xC0

/* Known Zorro manufacturer:product IDs */
#define HYDRA_AC_MANUF 0x0849
#define HYDRA_AC_PROD  0x01

int hydra_initialize();
static int hydraopen(), hydraclose(), hydrawput(), hydrawsrv();
static void hydraioctl();
static int hydraproto(), setup_ne2000();
static void receive_interrupt(), transmit_interrupt();
static void toss_packet_up_stream();
static void hydra_reset();

static struct module_info hydra_minfo =
{
    0x6879, "hya", HYDRA_MIN, HYDRA_MAX, HYDRA_HIWAT, HYDRA_LOWAT,
};

static struct qinit hydrarinit =
{
    NULL, NULL, hydraopen, hydraclose, NULL, &hydra_minfo, NULL,
};

static struct qinit hydrawinit =
{
    hydrawput, hydrawsrv, NULL, NULL, NULL, &hydra_minfo, NULL,
};

struct streamtab hydrainfo =
{
    &hydrarinit, &hydrawinit, NULL, NULL,
};

static hydra_board_t hydra_board[HYDRA_MAXBOARDS];
static hydra_autoconfig_t hydra_autoconfig[HYDRA_MAXBOARDS];
static int hydra_number_of_boards;

static unsigned char
hydra_inb(nic_base, reg)
volatile unsigned char *nic_base;
register int reg;
{
    return *(nic_base + (reg * 2));
}

static void
hydra_outb(nic_base, reg, val)
volatile unsigned char *nic_base;
int reg;
unsigned char val;
{
    *(nic_base + (reg * 2)) = val;
}

static void
hydra_rdma_read(nic_base, buf, len)
volatile unsigned char *nic_base;
unsigned char *buf;
int len;
{
    volatile unsigned char *rdma = nic_base;
    int i;

    for (i = 0; i < len; i++)
	*buf++ = *rdma;
}

static void
hydra_rdma_write(nic_base, buf, len)
volatile unsigned char *nic_base;
unsigned char *buf;
int len;
{
    volatile unsigned char *rdma = nic_base;
    int i;

    for (i = 0; i < len; i++)
	*rdma = *buf++;
}

static int
hydraopen(q, devp, flag, sflag, credp)
register queue_t *q;
dev_t *devp;
int flag, sflag;
cred_t *credp;
{
    register hydra_t *hp;
    register int minor_device, board_index, sap_index;
    hydra_board_t *board;

    if (sflag == MODOPEN || sflag == CLONEOPEN)
	return EINVAL;

    hydraautoconfig();

    minor_device = getminor(*devp);
    board_index = minor_device & 0x0F;
    sap_index = (minor_device & 0xF0) >> 4;
    board = &hydra_board[board_index];

    cmn_err(CE_NOTE, "hya%d: open (board_state=%d base=0x%08lx)",
	    board_index, board->hydra_status.board_state,
	    (unsigned long)board->hydra_info.board_base);

    if (board_index >= HYDRA_MAXBOARDS)
	return ENODEV;

    if (board->hydra_status.board_state != HYDRA_BOARD_RUNNING)
    {
	if (!hydra_initialize(board_index))
	{
	    cmn_err(CE_NOTE, "hya%d: init failed", board_index);
	    return ENODEV;
	}
	cmn_err(CE_NOTE, "hya%d: init OK at 0x%08lx", board_index,
		(unsigned long)board->hydra_info.board_base);
    }

    if (board->hydra_info.board_base == 0)
    {
	cmn_err(CE_NOTE, "hya%d: no board base", board_index);
	return ENXIO;
    }

    if (sap_index == 0)
    {
	for (sap_index = 0, hp = board->hydra;
	     sap_index < HYDRA_MAXDEV;
	     sap_index++, hp++)
	{
	    if (!hp->q)
		break;
	}
    }
    else
	--sap_index;

    if (sap_index >= HYDRA_MAXDEV)
	return ENOSPC;

    *devp = makedevice(getmajor(*devp), (((sap_index + 1) << 4) | board_index));

    hp = &board->hydra[sap_index];

    if (hp->q)
	return EBUSY;

    q->q_ptr = (char *)hp;
    WR(q)->q_ptr = (char *)hp;
    hp->q = q;
    WR(q)->q_ptr = (char *)hp;
    hp->board_index = board_index;
    hp->flags = 0;
    hp->state = DL_UNATTACHED;
    hp->sap = 0;

    if (!board->ifstats.ifs_name)
    {
	board->ifstats.ifs_name = "hya";
	board->ifstats.ifs_mtu = HYDRA_MTU;
	board->ifstats.ifs_unit = board_index;
	board->ifstats.ifs_next = ifstats;
	ifstats = &board->ifstats;
    }
    board->ifstats.ifs_active = 1;
    board->if_flags = IFF_BROADCAST | IFF_NOTRAILERS | IFF_RUNNING;

    return 0;
}

static int
hydraclose(q)
register queue_t *q;
{
    int dev;
    register hydra_t *hp = (hydra_t *)q->q_ptr;
    register hydra_board_t *board = &hydra_board[hp->board_index];

    hp->q = 0;

    OTHERQ(q)->q_ptr = NULL;
    q->q_ptr = NULL;

    for (dev = 0, hp = board->hydra; dev < HYDRA_MAXDEV; ++dev, ++hp)
    {
	if (hp->q)
	    break;
    }

    if (dev >= HYDRA_MAXDEV)
    {
	STRLOG(0x6879, 0, 1, SL_TRACE,
	       "hya[%d] Close called - halting NE2000\n",
	       hp->board_index);

	hydra_outb(board->hydra_info.nic_base, NE_CR,
		   NE_CR_P0 | NE_CR_STP | NE_CR_RDMA_ABORT);

	board->hydra_status.board_state = HYDRA_BOARD_RESET;
	board->ifstats.ifs_unit = 0;
	board->ifstats.ifs_active = 0;
	board->ifstats.ifs_ipackets = 0;
	board->ifstats.ifs_ierrors = 0;
	board->ifstats.ifs_opackets = 0;
	board->ifstats.ifs_oerrors = 0;
    }

    STRLOG(0x6879, 0, 1, SL_TRACE, "hya[%d] Closed\n",
	   board->hydra_info.board_base);

    return 0;
}

static int
hydrawput(q, mp)
register queue_t *q;
register mblk_t *mp;
{
    register int s;
    register hydra_t *hp = (hydra_t *)q->q_ptr;
    register hydra_board_t *board = &hydra_board[hp->board_index];

    s = splhydra();

    STRLOG(0x6879, 0, 8, SL_TRACE, "hya[%d] wput called\n", hp->board_index);

    switch (mp->b_datap->db_type)
    {
    case M_FLUSH:
	if (*mp->b_rptr & FLUSHW)
	{
	    flushq(q, FLUSHALL);
	    *mp->b_rptr &= ~FLUSHW;
	}

	if (*mp->b_rptr & FLUSHR)
	    qreply(q, mp);
	else
	    freemsg(mp);
	break;

    case M_PROTO:
    case M_PCPROTO:
	(void)hydraproto(q, mp);
	break;

    case M_IOCDATA:
    case M_IOCTL:
	hydraioctl(q, mp);
	break;

    default:
	STRLOG(0x6879, 0, 2, SL_ERROR | SL_WARN, "hya%d: bad msg type %d",
	       hp->board_index, mp->b_datap->db_type);
	freemsg(mp);
	break;
    }
    splx(s);
}

static int
hydrawsrv()
{
    return 0;
}

static int
hydraxmit(q, mp)
queue_t *q;
mblk_t *mp;
{
    register int n, pktsize;
    register hydra_t *hp = (hydra_t *)q->q_ptr;
    hydra_board_t *board = &hydra_board[hp->board_index];
    hydra_info_t *info = &board->hydra_info;
    volatile unsigned char *nic = info->nic_base;
    unsigned char tmp[ETH_MAXPACKET + 2];
    mblk_t *oldmp;
    dl_unitdata_req_t *dp = (dl_unitdata_req_t *)mp->b_rptr;
    unsigned char *ap;

    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_STA);
    if (hydra_inb(nic, NE_CR) & NE_CR_TXP)
	return 0;

    pktsize = sizeof(ether_header_t);
    ap = mp->b_rptr + dp->dl_dest_addr_offset;
    bcopy((caddr_t)ap, (caddr_t)&tmp[0], sizeof(ether_addr_t));
    bcopy((caddr_t)info->paddress, (caddr_t)&tmp[6], sizeof(ether_addr_t));
    bcopy((caddr_t)&hp->sap, (caddr_t)&tmp[12], sizeof(hp->sap));

    oldmp = mp;
    n = ETH_MAXDATA;

    for (mp = mp->b_cont; mp; mp = mp->b_cont)
    {
	int copy = min(mp->b_wptr - mp->b_rptr, n);
	ASSERT(copy >= 0);
	bcopy((caddr_t)mp->b_rptr, (caddr_t)&tmp[pktsize], copy);
	pktsize += copy;
	n -= copy;
    }

    freemsg(oldmp);

    if (pktsize < ETH_MINPACKET)
	pktsize = ETH_MINPACKET;

    info->tx_pkts_queued = 0;

    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_STA);
    hydra_outb(nic, NE_RSAR0, 0);
    hydra_outb(nic, NE_RSAR1, info->tx_start_page);
    hydra_outb(nic, NE_RBCNT0, pktsize & 0xff);
    hydra_outb(nic, NE_RBCNT1, (pktsize >> 8) & 0xff);
    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_STORE | NE_CR_STA);

    hydra_rdma_write(nic, tmp, pktsize);

    while (!(hydra_inb(nic, NE_ISR) & NE_ISR_RDC))
	;
    hydra_outb(nic, NE_ISR, NE_ISR_RDC);

    hydra_outb(nic, NE_TPSR, info->tx_start_page);
    hydra_outb(nic, NE_TBCNT0, pktsize & 0xff);
    hydra_outb(nic, NE_TBCNT1, (pktsize >> 8) & 0xff);
    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_TXP | NE_CR_STA);

    board->hydra_status.packets_sent++;
    board->ifstats.ifs_opackets++;

    STRLOG(0x6879, 0, 21, SL_TRACE,
	   "hya[%d] sent pkt len=%d\n", hp->board_index, pktsize);

    return 1;
}

static void
hydraioctl(q, mp)
queue_t *q;
mblk_t *mp;
{
    register struct iocblk *iocbp = (struct iocblk *)mp->b_rptr;
    register hydra_t *hp = (hydra_t *)q->q_ptr;
    register hydra_board_t *board = &hydra_board[hp->board_index];

    if (mp->b_datap->db_type == M_IOCDATA)
    {
	if (((struct copyresp *)mp->b_rptr)->cp_rval)
	{
	    freemsg(mp);
	    return;
	}

	if (!((struct copyresp *)mp->b_rptr)->cp_private)
	{
	    mp->b_datap->db_type = M_IOCACK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = 0;
	    putnext(RD(q), mp);
	    return;
	}
    }

    STRLOG(0x6879, 0, 7, SL_TRACE,
	   "hya[%d]: %s message, command 0x%x\n", hp->board_index,
	   (mp->b_datap->db_type == M_IOCDATA ? "IOCDATA" :
	    mp->b_datap->db_type == M_IOCTL ? "IOCTL" : "??"), iocbp->ioc_cmd);

    switch (iocbp->ioc_cmd)
    {
    case HYDRA_CLEAR_STATUS:
	board->hydra_status.packets_sent = 0;
	board->hydra_status.packets_received = 0;
	board->hydra_status.allocbs_failed = 0;
	board->hydra_status.couldnt_put = 0;
	board->hydra_status.tx_errors = 0;
	board->hydra_status.rx_errors = 0;
	board->hydra_status.collisions = 0;
	board->hydra_status.overflow = 0;
	board->hydra_status.crc_errors = 0;
	board->hydra_status.framing_errors = 0;
	board->hydra_status.missed_packets = 0;
	board->hydra_status.late_collisions = 0;
	board->hydra_status.loss_of_carrier = 0;
	board->hydra_status.retry_errors = 0;
	freemsg(mp->b_cont);
	break;

    case HYDRA_GET_STATUS:
    {
	hydra_status_t *status;
	caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;

	freemsg(mp->b_cont);

	mp->b_cont = allocb(sizeof(hydra_status_t), BPRI_MED);
	if (!mp->b_cont)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = ENOMEM;
	    putnext(RD(q), mp);
	    return;
	}

	status = (hydra_status_t *)mp->b_cont->b_rptr;
	mp->b_cont->b_wptr += sizeof(hydra_status_t);

	bcopy((caddr_t)&board->hydra_status, (caddr_t)status,
	      sizeof(hydra_status_t));

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYOUT;
	    creq->cq_addr = arg;
	    mp->b_wptr = mp->b_rptr + sizeof *creq;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(hydra_status_t);
	    creq->cq_size = sizeof(hydra_status_t);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)NULL;
	    putnext(RD(q), mp);
	    return;
	}

	break;
    }

    case HYDRA_GET_CONFIG:
    {
	hydra_config_t *hydra_config;
	caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;

	freemsg(mp->b_cont);

	mp->b_cont = allocb(sizeof(hydra_config_t), BPRI_MED);
	if (!mp->b_cont)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = ENOMEM;
	    putnext(RD(q), mp);
	    return;
	}

	hydra_config = (hydra_config_t *)mp->b_cont->b_rptr;
	mp->b_cont->b_wptr += sizeof(hydra_config_t);

	hydra_config->board_debug = board->hydra_status.board_debug;
	hydra_config->board_base = (long)board->hydra_info.board_base;
	hydra_config->mode = 0;
	hydra_config->flags = hp->flags;
	bcopy((caddr_t)board->hydra_info.paddress,
	      (caddr_t)hydra_config->paddress,
	      sizeof(board->hydra_info.paddress));
	bcopy((caddr_t)board->hydra_info.laddress,
	      (caddr_t)hydra_config->laddress,
	      sizeof(board->hydra_info.laddress));

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYOUT;
	    creq->cq_addr = arg;
	    mp->b_wptr = mp->b_rptr + sizeof *creq;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(hydra_config_t);
	    creq->cq_size = sizeof(hydra_config_t);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)NULL;
	    putnext(RD(q), mp);
	    return;
	}

	break;
    }

    case HYDRA_SET_CONFIG:
    {
	hydra_config_t *hydra_config;

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYIN;
	    creq->cq_addr = *(caddr_t *)mp->b_cont->b_rptr;
	    mp->b_wptr = mp->b_rptr + sizeof(*creq);
	    creq->cq_size = sizeof(*hydra_config);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)1;
	    putnext(RD(q), mp);
	    return;
	}
	else
	{
	    mblk_t *bp1;
	    register int i;

	    (void)pullupmsg(mp->b_cont, -1);

	    if (!mp->b_cont || blklen(mp->b_cont) != sizeof(*hydra_config))
	    {
		iocbp->ioc_error = EINVAL;
		mp->b_datap->db_type = M_IOCNAK;
		iocbp->ioc_count = 0;
		putnext(RD(q), mp);
		break;
	    }

	    hydra_config = (hydra_config_t *)mp->b_cont->b_rptr;

	    board->hydra_status.board_debug = hydra_config->board_debug;

	    mp->b_datap->db_type = M_IOCACK;

	    hp->flags = hydra_config->flags;

	    bp1 = unlinkb(mp);
	    if (bp1)
		freeb(bp1);
	    iocbp->ioc_count = 0;
	    putnext(RD(q), mp);

	    for (i = 0; i < HYDRA_MAXDEV; i++)
	    {
		dl_bind_ack_t *bind_ackp;
		dl_info_ack_t *info_ackp;

		if (board->hydra[i].sap && board->hydra[i].q)
		{
		    if (!(mp = allocb(sizeof(*bind_ackp) +
				    sizeof(board->hydra_info.paddress),
				    BPRI_MED)))
		    {
			return;
		    }

		    mp->b_datap->db_type = M_PCPROTO;
		    bind_ackp = (dl_bind_ack_t *)mp->b_wptr;
		    bind_ackp->dl_primitive = DL_BIND_ACK;
		    bind_ackp->dl_sap = hp->sap;
		    bind_ackp->dl_addr_length =
			sizeof(board->hydra_info.paddress);
		    bind_ackp->dl_addr_offset = sizeof(*bind_ackp);
		    bind_ackp->dl_max_conind = 0;
		    bind_ackp->dl_growth = 0;
		    mp->b_wptr += sizeof(*bind_ackp);

		    bcopy((caddr_t)board->hydra_info.paddress,
			  (caddr_t)mp->b_wptr,
			  sizeof(board->hydra_info.paddress));

		    mp->b_wptr += sizeof(board->hydra_info.paddress);

		    qreply(WR(board->hydra[i].q), mp);

		    if (!(mp = allocb(sizeof(dl_info_ack_t) +
				      sizeof(board->hydra_info.paddress),
				      BPRI_MED)))
		    {
			return;
		    }

		    mp->b_datap->db_type = M_PCPROTO;
		    info_ackp = (dl_info_ack_t *)mp->b_wptr;
		    info_ackp->dl_primitive = DL_INFO_ACK;
		    info_ackp->dl_max_sdu = ETH_MAXDATA;
		    info_ackp->dl_min_sdu = 1;
		    info_ackp->dl_addr_length =
			sizeof(board->hydra_info.paddress);
		    info_ackp->dl_mac_type = DL_ETHER;
		    info_ackp->dl_reserved = 0;
		    info_ackp->dl_current_state = hp->state;
		    info_ackp->dl_max_idu = ETH_MAXDATA;
		    info_ackp->dl_service_mode = DL_CLDLS;
		    info_ackp->dl_qos_length = 0;
		    info_ackp->dl_qos_offset = 0;
		    info_ackp->dl_qos_range_length = 0;
		    info_ackp->dl_qos_range_offset = 0;
		    info_ackp->dl_provider_style = DL_STYLE1;
		    info_ackp->dl_growth = 0;
		    if (hp->state == DL_IDLE)
		    {
			info_ackp->dl_addr_offset = sizeof(dl_info_ack_t);
			mp->b_wptr += sizeof(dl_info_ack_t);
			bcopy((caddr_t)board->hydra_info.paddress,
			      (caddr_t)mp->b_wptr,
			      sizeof(board->hydra_info.paddress));
			mp->b_wptr += sizeof(board->hydra_info.paddress);
		    }
		    else
		    {
			info_ackp->dl_addr_offset = 0;
			mp->b_wptr += sizeof(dl_info_ack_t);
		    }

		    qreply(WR(board->hydra[i].q), mp);
		}
	    }
	}
	break;
    }

    case HYDRA_NUMBER_OF_BOARDS:
    {
	caddr_t arg = *(caddr_t *)mp->b_cont->b_rptr;

	freemsg(mp->b_cont);

	mp->b_cont = allocb(sizeof(hydra_number_of_boards), BPRI_MED);
	if (!mp->b_cont)
	{
	    mp->b_datap->db_type = M_IOCNAK;
	    freemsg(unlinkb(mp));
	    iocbp->ioc_count = 0;
	    iocbp->ioc_rval = 0;
	    iocbp->ioc_error = ENOMEM;
	    putnext(RD(q), mp);
	    return;
	}

	mp->b_cont->b_wptr += sizeof(int);

	*(int *)mp->b_cont->b_rptr = hydra_number_of_boards;

	if (mp->b_datap->db_type == M_IOCTL && iocbp->ioc_count == TRANSPARENT)
	{
	    struct copyreq *creq = (struct copyreq *)mp->b_rptr;
	    mp->b_datap->db_type = M_COPYOUT;
	    creq->cq_addr = arg;
	    mp->b_wptr = mp->b_rptr + sizeof *creq;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(int);
	    creq->cq_size = sizeof(int);
	    creq->cq_flag = 0;
	    creq->cq_private = (mblk_t *)NULL;
	    putnext(RD(q), mp);
	    return;
	}

	break;
    }

    case SIOCSIFFLAGS:
	board->if_flags = ((struct ifreq *)mp->b_cont->b_rptr)->ifr_flags;
	mp->b_datap->db_type = M_IOCACK;
	freemsg(unlinkb(mp));
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = 0;
	putnext(RD(q), mp);
	return;

    case SIOCGIFFLAGS:
    {
	struct ifreq *ifrp = (struct ifreq *)mp->b_cont->b_rptr;
	cmn_err(CE_NOTE, "hya%d: SIOCGIFFLAGS (flags=0x%x state=%d)",
		hp->board_index, board->if_flags, hp->state);
	ifrp->ifr_flags = board->if_flags;
	mp->b_datap->db_type = M_IOCACK;
	iocbp->ioc_count = sizeof(struct ifreq);
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = 0;
	putnext(RD(q), mp);
	return;
    }

    case IF_UNITSEL:
    default:
	mp->b_datap->db_type = M_IOCNAK;
	freemsg(unlinkb(mp));
	iocbp->ioc_count = 0;
	iocbp->ioc_rval = 0;
	iocbp->ioc_error = EINVAL;
	putnext(RD(q), mp);
	return;
    }

    mp->b_datap->db_type = M_IOCACK;
    freemsg(unlinkb(mp));
    iocbp->ioc_count = 0;
    iocbp->ioc_rval = 0;
    iocbp->ioc_error = 0;
    putnext(RD(q), mp);
    return;
}

static int
hydraproto(q, mp)
queue_t *q;
mblk_t *mp;
{
    register union DL_primitives *p = (union DL_primitives *)mp->b_rptr;
    register hydra_t *hp = (hydra_t *)q->q_ptr;
    register hydra_board_t *board = &hydra_board[hp->board_index];

    STRLOG(0x6879, 0, 8, SL_TRACE, "hya[%d] proto called (%d)\n",
	   hp->board_index, p->dl_primitive);

    switch (p->dl_primitive)
    {
    case DL_UNITDATA_REQ:
	if (q->q_first || !hydraxmit(q, mp))
	    putq(q, mp);
	break;

    case DL_ATTACH_REQ:
	{
	    dl_ok_ack_t *okp;

	    cmn_err(CE_NOTE, "hya%d: DL_ATTACH_REQ (state=%d)",
		    hp->board_index, hp->state);

	    STRLOG(0x6879, 0, 8, SL_TRACE, "hya[%d] Attach Request\n",
		   hp->board_index);

	    freemsg(mp);

	    if (!(mp = allocb(sizeof(dl_ok_ack_t), BPRI_MED)))
		return 1;

	    okp = (dl_ok_ack_t *)mp->b_wptr;
	    mp->b_datap->db_type = M_PCPROTO;
	    okp->dl_primitive = DL_OK_ACK;
	    okp->dl_correct_primitive = DL_ATTACH_REQ;
	    mp->b_wptr += sizeof(dl_ok_ack_t);

	    hp->state = DL_UNBOUND;

	    qreply(q, mp);
	    return 1;
	}

    case DL_BIND_REQ:
	{
	    dl_bind_req_t *reqp = (dl_bind_req_t *)mp->b_rptr;
	    dl_bind_ack_t *ackp;

	    cmn_err(CE_NOTE, "hya%d: DL_BIND_REQ sap=0x%x (state=%d)",
		    hp->board_index, reqp->dl_sap, hp->state);

	    hp->sap = reqp->dl_sap;
	    hp->state = DL_IDLE;

	    STRLOG(0x6879, 0, 8, SL_TRACE, "hya[%d] Setting sap to 0x%x\n",
		   hp->board_index, hp->sap);

	    freemsg(mp);

	    if (!(mp = allocb(sizeof(*ackp) + sizeof(board->hydra_info.paddress),
			    BPRI_MED)))
		return 1;

	    mp->b_datap->db_type = M_PCPROTO;
	    ackp = (dl_bind_ack_t *)mp->b_wptr;
	    ackp->dl_primitive = DL_BIND_ACK;
	    ackp->dl_sap = hp->sap;
	    ackp->dl_addr_length = sizeof(board->hydra_info.paddress);
	    ackp->dl_addr_offset = sizeof(*ackp);
	    ackp->dl_max_conind = 0;
	    ackp->dl_growth = 0;
	    mp->b_wptr += sizeof(*ackp);

	    bcopy((caddr_t)board->hydra_info.paddress, (caddr_t)mp->b_wptr,
		  sizeof(board->hydra_info.paddress));

	    mp->b_wptr += sizeof(board->hydra_info.paddress);

	    qreply(q, mp);
	    return 1;
	}

    case DL_INFO_REQ:
	{
	    dl_info_ack_t *ackp;

	    cmn_err(CE_NOTE, "hya%d: DL_INFO_REQ (state=%d sap=0x%x style=%d)",
		    hp->board_index, hp->state, hp->sap, DL_STYLE1);

	    freemsg(mp);

	    if (!(mp = allocb(sizeof(dl_info_ack_t) +
			      sizeof(board->hydra_info.paddress), BPRI_MED)))
		return 1;

	    mp->b_datap->db_type = M_PCPROTO;
	    ackp = (dl_info_ack_t *)mp->b_wptr;
	    ackp->dl_primitive = DL_INFO_ACK;
	    ackp->dl_max_sdu = ETH_MAXDATA;
	    ackp->dl_min_sdu = 1;
	    ackp->dl_addr_length = sizeof(board->hydra_info.paddress);
	    ackp->dl_mac_type = DL_ETHER;
	    ackp->dl_reserved = 0;
	    ackp->dl_current_state = hp->state;
	    ackp->dl_max_idu = ETH_MAXDATA;
	    ackp->dl_service_mode = DL_CLDLS;
	    ackp->dl_qos_length = 0;
	    ackp->dl_qos_offset = 0;
	    ackp->dl_qos_range_length = 0;
	    ackp->dl_qos_range_offset = 0;
	    ackp->dl_provider_style = DL_STYLE1;
	    ackp->dl_growth = 0;
	    if (hp->state != DL_UNATTACHED)
	    {
		ackp->dl_addr_offset = sizeof(dl_info_ack_t);
		mp->b_wptr += sizeof(dl_info_ack_t);
		bcopy((caddr_t)board->hydra_info.paddress,
		      (caddr_t)mp->b_wptr,
		      sizeof(board->hydra_info.paddress));
		mp->b_wptr += sizeof(board->hydra_info.paddress);
	    }
	    else
	    {
		ackp->dl_addr_offset = 0;
		mp->b_wptr += sizeof(dl_info_ack_t);
	    }

	    qreply(q, mp);
	    return 1;
	}

    case DL_UNBIND_REQ:
	{
	    dl_ok_ack_t *okp;

	    STRLOG(0x6879, 0, 8, SL_TRACE, "hya[%d] Unbind Request, sap=0x%x\n",
		   hp->board_index, hp->sap);

	    freemsg(mp);

	    if (!(mp = allocb(sizeof(dl_ok_ack_t), BPRI_MED)))
		return 1;

	    okp = (dl_ok_ack_t *)mp->b_wptr;
	    mp->b_datap->db_type = M_PCPROTO;
	    okp->dl_primitive = DL_OK_ACK;
	    okp->dl_correct_primitive = DL_UNBIND_REQ;
	    mp->b_wptr += sizeof(dl_ok_ack_t);

	    hp->state = DL_UNBOUND;
	    hp->sap = 0;

	    qreply(q, mp);
	    return 1;
	}

    case DL_DETACH_REQ:
	{
	    dl_ok_ack_t *okp;

	    STRLOG(0x6879, 0, 8, SL_TRACE, "hya[%d] Detach Request\n",
		   hp->board_index);

	    freemsg(mp);

	    if (!(mp = allocb(sizeof(dl_ok_ack_t), BPRI_MED)))
		return 1;

	    okp = (dl_ok_ack_t *)mp->b_wptr;
	    mp->b_datap->db_type = M_PCPROTO;
	    okp->dl_primitive = DL_OK_ACK;
	    okp->dl_correct_primitive = DL_DETACH_REQ;
	    mp->b_wptr += sizeof(dl_ok_ack_t);

	    hp->state = DL_UNATTACHED;

	    qreply(q, mp);
	    return 1;
	}

    default:
	STRLOG(0x6879, 0, 2, SL_ERROR | SL_WARN,
	       "hydraproto: unknown proto request: %d", (int)p->dl_primitive);
	freemsg(mp);
	return 1;
    }

    return 0;
}

hydraintr()
{
    unsigned char status;
    int board_index;

    if (panicstr)
	return;

    for (board_index = 0; board_index < HYDRA_MAXBOARDS; ++board_index)
    {
	hydra_board_t *board = &hydra_board[board_index];

	if (board->hydra_status.board_state == HYDRA_BOARD_RUNNING &&
	    board->hydra_info.nic_base != 0)
	{
	    volatile unsigned char *nic = board->hydra_info.nic_base;

	    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_STA);
	    status = hydra_inb(nic, NE_ISR);

	    if (status == 0)
		continue;

	    STRLOG(0x6879, 0, 6, SL_TRACE, "hya[%d] INT; ISR = %x\n",
		   board_index, status);

	    if (status & (NE_ISR_PRX | NE_ISR_RXE | NE_ISR_OVW))
	    {
		if (status & NE_ISR_PRX)
		    receive_interrupt(board_index);

		if (status & NE_ISR_RXE)
		{
		    board->hydra_status.rx_errors++;
		    board->ifstats.ifs_ierrors++;
		}

		if (status & NE_ISR_OVW)
		{
		    board->hydra_status.overflow++;
		    board->ifstats.ifs_ierrors++;
		}

		hydra_outb(nic, NE_ISR,
			   status & (NE_ISR_PRX | NE_ISR_RXE | NE_ISR_OVW));
	    }

	    if (status & NE_ISR_PTX)
	    {
		hydra_outb(nic, NE_ISR, NE_ISR_PTX);
		transmit_interrupt(board_index);
	    }

	    if (status & NE_ISR_TXE)
	    {
		unsigned char tsr;
		tsr = hydra_inb(nic, NE_TSR);
		hydra_outb(nic, NE_ISR, NE_ISR_TXE);
		board->hydra_status.tx_errors++;
		board->ifstats.ifs_oerrors++;
		if (tsr & NE_TSR_COL)
		    board->hydra_status.collisions++;
		if (tsr & NE_TSR_ABT)
		    board->hydra_status.retry_errors++;
		if (tsr & NE_TSR_CDH)
		    board->hydra_status.late_collisions++;
		if (tsr & NE_TSR_CRS)
		    board->hydra_status.loss_of_carrier++;
	    }

	    if (status & NE_ISR_CNT)
	    {
		hydra_outb(nic, NE_ISR, NE_ISR_CNT);
		board->hydra_status.missed_packets += hydra_inb(nic, NE_CNTR0);
	    }
	}
    }
}

static void
receive_interrupt(board_index)
int board_index;
{
    hydra_board_t *board = &hydra_board[board_index];
    hydra_info_t *info = &board->hydra_info;
    volatile unsigned char *nic = info->nic_base;
    static unsigned char rxbuff[MAX_BUFFER_LENGTH];
    int curr, boundary, pktsize;
    unsigned char rsr;
    int next_frame;

    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_STA);

    while (1)
    {
	hydra_outb(nic, NE_CR, NE_CR_P1 | NE_CR_STA);
	curr = hydra_inb(nic, NE_CURR);
	hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_STA);
	boundary = hydra_inb(nic, NE_BNRY);

	next_frame = info->next_pkt;

	if (next_frame == curr)
	    break;

	hydra_outb(nic, NE_RSAR0, 0);
	hydra_outb(nic, NE_RSAR1, next_frame);
	hydra_outb(nic, NE_RBCNT0, 4);
	hydra_outb(nic, NE_RBCNT1, 0);
	hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_PULL | NE_CR_STA);

	{
	    unsigned char header[4];
	    hydra_rdma_read(nic, header, 4);
	    while (!(hydra_inb(nic, NE_ISR) & NE_ISR_RDC))
		;
	    hydra_outb(nic, NE_ISR, NE_ISR_RDC);

	    rsr = header[0];
	    next_frame = header[1];
	    pktsize = ((unsigned int)header[3] << 8) | header[2];
	}

	if (!(rsr & NE_RSR_PRX))
	{
	    board->hydra_status.rx_errors++;
	    board->ifstats.ifs_ierrors++;
	    info->next_pkt = next_frame;
	    hydra_outb(nic, NE_BNRY,
		       (next_frame - 1 + NE8390_STOP_PG) & (NE8390_STOP_PG - 1));
	    continue;
	}

	pktsize -= 4;

	if (pktsize < ETH_MINPACKET || pktsize > MAX_BUFFER_LENGTH)
	{
	    board->hydra_status.buffer_error++;
	    info->next_pkt = next_frame;
	    hydra_outb(nic, NE_BNRY,
		       (next_frame - 1 + NE8390_STOP_PG) & (NE8390_STOP_PG - 1));
	    continue;
	}

	{
	    int offset = info->next_pkt * 256;
	    int stop = NE8390_STOP_PG * 256;

	    if (offset + pktsize > stop)
	    {
		int semi = stop - offset;

		hydra_outb(nic, NE_RSAR0, 0);
		hydra_outb(nic, NE_RSAR1, info->next_pkt);
		hydra_outb(nic, NE_RBCNT0, semi & 0xff);
		hydra_outb(nic, NE_RBCNT1, (semi >> 8) & 0xff);
		hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_PULL | NE_CR_STA);
		hydra_rdma_read(nic, rxbuff, semi);
		while (!(hydra_inb(nic, NE_ISR) & NE_ISR_RDC))
		    ;
		hydra_outb(nic, NE_ISR, NE_ISR_RDC);

		hydra_outb(nic, NE_RSAR0, 0);
		hydra_outb(nic, NE_RSAR1, info->rx_start_page);
		hydra_outb(nic, NE_RBCNT0, (pktsize - semi) & 0xff);
		hydra_outb(nic, NE_RBCNT1, ((pktsize - semi) >> 8) & 0xff);
		hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_PULL | NE_CR_STA);
		hydra_rdma_read(nic, rxbuff + semi, pktsize - semi);
		while (!(hydra_inb(nic, NE_ISR) & NE_ISR_RDC))
		    ;
		hydra_outb(nic, NE_ISR, NE_ISR_RDC);
	    }
	    else
	    {
		hydra_outb(nic, NE_RSAR0, 0);
		hydra_outb(nic, NE_RSAR1, info->next_pkt);
		hydra_outb(nic, NE_RBCNT0, pktsize & 0xff);
		hydra_outb(nic, NE_RBCNT1, (pktsize >> 8) & 0xff);
		hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_PULL | NE_CR_STA);
		hydra_rdma_read(nic, rxbuff, pktsize);
		while (!(hydra_inb(nic, NE_ISR) & NE_ISR_RDC))
		    ;
		hydra_outb(nic, NE_ISR, NE_ISR_RDC);
	    }
	}

	board->hydra_status.packets_received++;
	board->ifstats.ifs_ipackets++;

	if (rsr & NE_RSR_CRC)
	    board->hydra_status.crc_errors++;
	if (rsr & NE_RSR_FAE)
	    board->hydra_status.framing_errors++;
	if (rsr & NE_RSR_FO)
	    board->hydra_status.overflow++;

	info->next_pkt = next_frame;
	hydra_outb(nic, NE_BNRY,
		   (next_frame - 1 + NE8390_STOP_PG) & (NE8390_STOP_PG - 1));

	toss_packet_up_stream(rxbuff, board_index, pktsize);
    }
}

static void
toss_packet_up_stream(packet, board_index, count)
char *packet;
int board_index, count;
{
    int j, length, trailn;
    unsigned short sap;
    hydra_board_t *board = &hydra_board[board_index];
    hydra_info_t *info = &board->hydra_info;
    hydra_t *hp = board->hydra;
    static unsigned char broadcast_address[6] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    ether_header_t *packet_header = (ether_header_t *)packet;

    sap = packet_header->ether_type;

    if (sap >= ETHERTYPE_TRAIL &&
	sap <= (unsigned short)(ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER))
    {
	length = (sap - ETHERTYPE_TRAIL) * 512;
	trailn = (*((unsigned short *)
		    &packet[sizeof(ether_header_t) + length + ETH_TYPE_SIZE]) -
		  ETH_TYPE_SIZE - ETH_LENGTH_SIZE);
	sap = *((unsigned short *)&packet[sizeof(ether_header_t) + length]);
	board->hydra_status.trailers++;
    }
    else
    {
	length = count - ETH_CRC_LEN;
	trailn = 0;
    }

    for (j = 0; j < HYDRA_MAXDEV; j++, hp++)
    {
	mblk_t *mp = (mblk_t *)NULL;

	if (!hp->q)
	    continue;

	if (hp->flags & HYDRA_RAW_DATA)
	{
	    if (canput(hp->q->q_next))
	    {
		if ((mp = allocb(sizeof(hydra_status_t), BPRI_MED)) != NULL &&
		    (mp->b_cont = allocb(count, BPRI_MED)) != NULL)
		{
		    mp->b_datap->db_type = M_PROTO;
		    bcopy((caddr_t)&board->hydra_status,
			  (caddr_t)mp->b_wptr, sizeof(board->hydra_status));
		    mp->b_wptr += sizeof(board->hydra_status);

		    mp->b_cont->b_datap->db_type = M_DATA;
		    bcopy((caddr_t)packet, (caddr_t)mp->b_cont->b_wptr, count);
		    mp->b_cont->b_wptr += count;
		    putnext(hp->q, mp);
		}
		else
		{
		    board->hydra_status.allocbs_failed++;
		    if (mp)
			freemsg(mp);
		}
	    }
	    else
		board->hydra_status.couldnt_put++;
	}
	else if (hp->sap == sap)
	{
	    if (bcmp((char *)packet_header->ether_dhost,
		     (char *)info->paddress,
		     sizeof(ether_addr_t)) &&
		bcmp((char *)packet_header->ether_dhost,
		     (char *)broadcast_address,
		     sizeof(broadcast_address)))
		continue;

	    if (canput(hp->q->q_next))
	    {
		if ((mp = allocb(sizeof(union DL_primitives) +
			       (2 * sizeof(ether_addr_t)),
			       BPRI_MED)) != NULL &&
		    (mp->b_cont = allocb(length + trailn, BPRI_MED)) != NULL)
		{
		    register dl_unitdata_ind_t
			*dp = (dl_unitdata_ind_t *)mp->b_wptr;

		    dp->dl_primitive = DL_UNITDATA_IND;
		    dp->dl_dest_addr_length = sizeof(ether_addr_t);
		    dp->dl_dest_addr_offset = sizeof(union DL_primitives);
		    dp->dl_src_addr_length = sizeof(ether_addr_t);
		    dp->dl_src_addr_offset = sizeof(union DL_primitives) +
			sizeof(ether_addr_t);

		    bcopy((caddr_t)&packet_header->ether_dhost,
			  (caddr_t)mp->b_wptr + dp->dl_dest_addr_offset,
			  sizeof(ether_addr_t));

		    bcopy((caddr_t)&packet_header->ether_shost,
			  (caddr_t)mp->b_wptr + dp->dl_src_addr_offset,
			  sizeof(ether_addr_t));

		    dp->dl_reserved = 0;
		    mp->b_wptr += sizeof(union DL_primitives) +
			(2 * sizeof(ether_addr_t));

		    mp->b_datap->db_type = M_PROTO;

		    bcopy((caddr_t)&packet[sizeof(ether_header_t)],
			  (caddr_t)mp->b_cont->b_wptr,
			  length);

		    mp->b_cont->b_wptr += length;
		    mp->b_cont->b_datap->db_type = M_DATA;

		    putnext(hp->q, mp);
		}
		else
		{
		    board->hydra_status.allocbs_failed++;
		    if (mp)
			freemsg(mp);
		}
	    }
	    else
		board->hydra_status.couldnt_put++;
	}
    }
}

static void
transmit_interrupt(board_index)
int board_index;
{
    hydra_t *hp = hydra_board[board_index].hydra;
    int n;
    mblk_t *mp;

    for (n = 0; n < HYDRA_MAXDEV; n++, hp++)
    {
	if (hp->q)
	{
	    queue_t *wrq = WR(hp->q);

	    mp = getq(wrq);

	    if (mp && !hydraxmit(wrq, mp))
	    {
		putbq(wrq, mp);
		break;
	    }
	}
    }
}

static void
get_ethernet_address(base, physical_ethernet_address)
long base;
unsigned char physical_ethernet_address[6];
{
    register volatile unsigned char *ptr;
    int j;

    ptr = (unsigned char *)(base + NE8390_ADDRPROM_OFFSET);

    for (j = 0; j < 6; j++)
	physical_ethernet_address[j] = *(ptr + j * 2);
}

/* AutoConfig nibble decode helpers */
static unsigned char
ac_byte(volatile unsigned char *mem, int n)
{
    unsigned char hi = (mem[n * 4    ] >> 4) & 0x0F;
    unsigned char lo = (mem[n * 4 + 2] >> 4) & 0x0F;
    unsigned char raw = (hi << 4) | lo;
    return (n == AC_TYPE) ? raw : (unsigned char)(~raw & 0xFF);
}

static unsigned short
ac_word(volatile unsigned char *mem, int n)
{
    return (unsigned short)((unsigned short)ac_byte(mem, n) << 8 | ac_byte(mem, n + 1));
}

static int
ac_match_board(volatile unsigned char *mem)
{
    unsigned char er_type;
    int i;

    /* Reject bus float */
    for (i = 0; i < 16; i++)
	if (mem[i] != 0xFF) break;
    if (i == 16) return 0;
    for (i = 0; i < 16; i++)
	if (mem[i] != 0x00) break;
    if (i == 16) return 0;

    /* Must be Zorro II type */
    er_type = ac_byte(mem, AC_TYPE);
    if ((er_type & ERT_TYPEMASK) != ERT_ZORROII)
	return 0;

    /* Reserved byte must decode to 0x00 */
    if (ac_byte(mem, AC_RESV) != 0x00)
	return 0;

    /* Check manufacturer and product */
    {
	unsigned short manuf = ac_word(mem, AC_MANUF);
	unsigned char prod   = ac_byte(mem, AC_PRODUCT);
	return (manuf == HYDRA_AC_MANUF && prod == HYDRA_AC_PROD);
    }
}

void
hydraautoconfig()
{
    int i, slot, n;
    long addr, size;
    static int hydraautoconfigured;

    if (hydraautoconfigured)
	return;

    hydraautoconfigured = 1;
    hydra_number_of_boards = 0;
    cmn_err(CE_NOTE, "hydra: probing for Hydra Systems AmigaNet (DP8390)");

    /* Method 1: autocon() using bootinfo table */
    n = 0;
    for (i = 0; i < HYDRA_MAXBOARDS; i++)
    {
	if (autocon(NE8390_BOARD_ID, i, &addr, &size))
	{
	    hydra_autoconfig[hydra_number_of_boards].address = addr;
	    hydra_autoconfig[hydra_number_of_boards].type = 1;
	    hydra_number_of_boards++;
	    n++;
	}
	else
	    break;
    }
    if (n > 0)
    {
	cmn_err(CE_NOTE, "hydra: found %d board(s) via autocon", n);
	for (i = 0; i < n; i++)
	    cmn_err(CE_NOTE, "hydra:   board %d at 0x%08lx",
		    i, hydra_autoconfig[i].address);
	return;
    }

    /* Method 1a: Direct AutoConfig ROM decode at Zorro II I/O slots */
    n = 0;
    for (slot = 0; slot < 8; slot++)
    {
	unsigned long base = 0x00E90000UL + slot * 0x10000UL;
	volatile unsigned char *mem = (volatile unsigned char *)base;

	if (ac_match_board(mem))
	{
	    unsigned short manuf = ac_word(mem, AC_MANUF);
	    unsigned char prod   = ac_byte(mem, AC_PRODUCT);

	    cmn_err(CE_NOTE, "hydra: slot %d AutoConfig %04X:%02X at 0x%08lx",
		    slot, manuf, prod, base);

	    if (hydra_number_of_boards < HYDRA_MAXBOARDS)
	    {
		hydra_autoconfig[hydra_number_of_boards].address = base;
		hydra_autoconfig[hydra_number_of_boards].type = 1;
		hydra_number_of_boards++;
		n++;
	    }
	}
    }
    if (n > 0)
    {
	cmn_err(CE_NOTE, "hydra: found %d board(s) via AutoConfig decode", n);
	for (i = 0; i < n; i++)
	    cmn_err(CE_NOTE, "hydra:   board %d at 0x%08lx",
		    i, hydra_autoconfig[i].address);
	return;
    }

    /* Method 2: Direct Zorro II slot probe (MAC at +0xffc0) */
    n = 0;
    for (slot = 0; slot < 8; slot++)
    {
	unsigned long base = 0x00E80000UL + slot * 0x10000UL;
	unsigned char mac[6];
	int valid;

	for (i = 0; i < 6; i++)
	    mac[i] = *(volatile unsigned char *)(base + 0xffc0 + i * 2);

	valid = 1;
	for (i = 0; i < 6; i++)
	    if (mac[i] != 0xFF) break;
	if (i == 6) valid = 0;            /* all 0xFF = bus float */
	for (i = 0; i < 6; i++)
	    if (mac[i] != 0x00) break;
	if (i == 6) valid = 0;            /* all 0x00 = no PROM */
	for (i = 1; i < 6; i++)
	    if (mac[i] != mac[0]) break;
	if (i == 6) valid = 0;            /* all same = invalid */

	if (!valid)
	    continue;

	cmn_err(CE_NOTE, "hydra: slot %d (base 0x%08lx) MAC %02x:%02x:%02x:%02x:%02x:%02x",
		slot, base, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (hydra_number_of_boards < HYDRA_MAXBOARDS)
	{
	    hydra_autoconfig[hydra_number_of_boards].address = base;
	    hydra_autoconfig[hydra_number_of_boards].type = 1;
	    hydra_number_of_boards++;
	    n++;
	}
    }
    if (n > 0)
    {
	cmn_err(CE_NOTE, "hydra: found %d board(s) via Zorro II probe", n);
	for (i = 0; i < n; i++)
	    cmn_err(CE_NOTE, "hydra:   board %d at 0x%08lx",
		    i, hydra_autoconfig[i].address);
	return;
    }

    /* Method 3: A2065 fallback for FS-UAE testing */
    n = 0;
    for (i = 0; i < HYDRA_MAXBOARDS; i++)
    {
	if (autocon(0x02020070, i, &addr, &size))
	{
	    cmn_err(CE_NOTE, "hydra: found A2065 at 0x%08lx (testing mode)", addr);
	    hydra_autoconfig[hydra_number_of_boards].address = addr;
	    hydra_autoconfig[hydra_number_of_boards].type = 0x02020070;
	    hydra_number_of_boards++;
	    n++;
	}
	else
	    break;
    }
    if (n > 0)
    {
	cmn_err(CE_NOTE, "hydra: found %d board(s) via A2065 fallback", n);
	return;
    }

    cmn_err(CE_NOTE, "hydra: no board found");
}

int
hydra_initialize(board_index)
int board_index;
{
    int n, type;
    hydra_t *hp;
    unsigned char paddress[6];
    hydra_board_t *board = &hydra_board[board_index];

    board->hydra_info.board_base =
	(unsigned char *)hydra_autoconfig[board_index].address;
    if (board->hydra_info.board_base == 0)
	return 0;

    type = hydra_autoconfig[board_index].type;

    if (type == 1)
    {
	/* Real Hydra / NE8390 board */
	board->hydra_info.nic_base =
	    board->hydra_info.board_base + NE8390_NIC_OFFSET;
	get_ethernet_address((long)board->hydra_info.board_base, paddress);
    }
    else
    {
	/* Non-Hydra board (e.g. A2065 for FS-UAE testing).
	 * Store a fake address so STREAMS/DLPI plumbing can be tested.
	 */
	board->hydra_info.nic_base = 0;
	paddress[0] = 0x02;
	paddress[1] = 0x00;
	paddress[2] = 0x68;
	paddress[3] = 0x79;
	paddress[4] = 0x00;
	paddress[5] = board_index;
    }

    for (n = 0, hp = board->hydra; n < HYDRA_MAXDEV; ++n, ++hp)
    {
	hp->q = (queue_t *)NULL;
	hp->sap = 0;
	hp->state = DL_UNBOUND;
    }

    if (type != 1)
    {
	/* Fake board for testing — mark as running without NE2000 init */
	board->hydra_status.board_state = HYDRA_BOARD_RUNNING;
	bcopy((caddr_t)paddress, (caddr_t)board->hydra_info.paddress,
	      sizeof(board->hydra_info.paddress));
	return 1;
    }

    return setup_ne2000(board_index, paddress);
}

static int
setup_ne2000(board_index, ethernet_address)
int board_index;
unsigned char ethernet_address[6];
{
    register int i;
    hydra_board_t *board = &hydra_board[board_index];
    hydra_info_t *info = &board->hydra_info;
    volatile unsigned char *nic = info->nic_base;

    STRLOG(0x6879, 0, 14, SL_TRACE,
	   "hya[%d] Address: %x:%x:%x:%x:%x:%x\n",
	   board_index,
	   ethernet_address[0], ethernet_address[1],
	   ethernet_address[2], ethernet_address[3],
	   ethernet_address[4], ethernet_address[5]);

    bcopy((caddr_t)ethernet_address, (caddr_t)info->paddress,
	  sizeof(info->paddress));

    info->tx_start_page = NE8390_START_PG;
    info->rx_start_page = NE8390_RX_START_PG;
    info->stop_page = NE8390_STOP_PG;
    info->next_pkt = NE8390_RX_START_PG;

    hydra_reset(board_index);

    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_STP | NE_CR_RDMA_ABORT);
    DELAY(10);

    hydra_outb(nic, NE_DCR, NE_DCR_BOS | NE_DCR_LAS | NE_DCR_FT1);

    hydra_outb(nic, NE_RBCNT0, 0);
    hydra_outb(nic, NE_RBCNT1, 0);

    hydra_outb(nic, NE_PSTART, info->rx_start_page);
    hydra_outb(nic, NE_PSTOP, info->stop_page);
    hydra_outb(nic, NE_BNRY, info->stop_page - 1);

    hydra_outb(nic, NE_ISR, 0xff);

    hydra_outb(nic, NE_CR, NE_CR_P1 | NE_CR_STP);
    for (i = 0; i < 6; i++)
	hydra_outb(nic, NE_PAR0 + i, ethernet_address[i]);
    for (i = 0; i < 8; i++)
	hydra_outb(nic, NE_MAR0 + i, 0);
    hydra_outb(nic, NE_CURR, info->rx_start_page);

    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_STA);
    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_RDMA_ABORT | NE_CR_STA);

    board->hydra_status.board_state = HYDRA_BOARD_RUNNING;
    board->hydra_status.packets_sent = 0;
    board->hydra_status.packets_received = 0;
    board->hydra_status.allocbs_failed = 0;
    board->hydra_status.couldnt_put = 0;
    board->hydra_status.tx_errors = 0;
    board->hydra_status.rx_errors = 0;
    board->hydra_status.collisions = 0;
    board->hydra_status.overflow = 0;
    board->hydra_status.crc_errors = 0;
    board->hydra_status.framing_errors = 0;
    board->hydra_status.missed_packets = 0;
    board->hydra_status.trailers = 0;
    board->hydra_status.buffer_error = 0;
    board->hydra_status.late_collisions = 0;
    board->hydra_status.loss_of_carrier = 0;
    board->hydra_status.retry_errors = 0;

    return 1;
}

static void
hydra_reset(board_index)
int board_index;
{
    hydra_board_t *board = &hydra_board[board_index];
    volatile unsigned char *nic = board->hydra_info.nic_base;
    int i;

    hydra_outb(nic, NE_CR, NE_CR_P0 | NE_CR_STP | NE_CR_RDMA_ABORT);
    for (i = 0; i < 100; i++)
    {
	DELAY(1);
	if (hydra_inb(nic, NE_ISR) & NE_ISR_RST)
	    break;
    }

    hydra_outb(nic, NE_ISR, 0xff);

    for (i = 0; i < 100; i++)
    {
	DELAY(1);
	if (!(hydra_inb(nic, NE_ISR) & NE_ISR_RST))
	    break;
    }

    DELAY(100);
}

void
hydrainit()
{
    volatile unsigned short *cfg;
    unsigned short id;

    /* Existing autoconfig probe — prints Hydra / A2065 results */
    hydraautoconfig();

    /*
     * ZZ9000 probe via known Z2 base address (0xE80000).
     * The ZZ9000 uses a non-standard autoconfig layout, so autocon()
     * cannot find it.  We check the first autoconfig word directly.
     */
    cfg = (volatile unsigned short *)0x00E80000;
    id = *cfg;
    if (id != 0xFFFF && id != 0x0000)
	cmn_err(CE_NOTE, "hydra: ZZ9000 (MNT Research) net/rtg at 0xE80000 (id=0x%04x)", id);

    /*
     * Also scan Z2 I/O slots for a ZZ9000 in a non-default slot.
     */
    {
	int slot;
	for (slot = 0; slot < 8; slot++)
	{
	    cfg = (volatile unsigned short *)(0x00E90000UL + slot * 0x10000UL);
	    id = *cfg;
	    if (id != 0xFFFF && id != 0x0000)
		cmn_err(CE_NOTE, "hydra: unknown Z2 board at 0x%08lx (id=0x%04x)",
			(unsigned long)cfg, id);
	}
    }
}

