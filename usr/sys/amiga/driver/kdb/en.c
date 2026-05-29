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
#include "sys/inline.h"
#include "sys/systm.h"
#include "sys/cred.h"
#include "sys/dlpi.h"
#include "sys/ddi.h"
#include "aenuser.h"
#include "aen.h"
#include "lance.h"
#include "kdebug.h"

extern aen_board_t aen_board[];

/*
** Send_KDB_Packet: Send a kernel debug packet over the line (ethernet
** aen device).  This function has device driver specific information
** about the ethernet board so it can put a packet in the ethernet
** board's (lance based) transfer buffer.
**
** This is used on the debugged system to send a reply to a kernel debug
** packet.
*/
void
Send_KDB_Packet(response, device, dst_addr)
register kdebug_t *response;
int device;
ether_addr_t dst_addr;
{
    register int i;
    register aen_board_t *board = &aen_board[device];

#ifdef DEBUG
	kdbhexdump(response, sizeof(*response), "Sending KDB packet\n");
#endif /* DEBUG */

    i = board->aen_info.tx_buffer_index;

    if (!(board->aen_info.txptr[i].mode&TMD1_OWN))
    {
	struct ether_header *ebuf = (struct ether_header *)
	    (board->aen_info.board_base + board->aen_info.txptr[i].loaddr);
	/*
	** The current buffer is free.  Set the source ethernet
	** address and copy the output buffer to the transmit buffer.
	*/

	bcopy((caddr_t)dst_addr,
	      (caddr_t)ebuf->ether_dhost, sizeof(ebuf->ether_dhost));

	bcopy((caddr_t)board->aen_info.paddress,
	      (caddr_t)ebuf->ether_shost, sizeof(ebuf->ether_shost));

	ebuf->ether_type = ETHERTYPE_KERNEL_DEBUG;

	bcopy((caddr_t)response,
	      (caddr_t)(board->aen_info.board_base +
			board->aen_info.txptr[i].loaddr + ETH_HEADER_SIZE),
	      sizeof(*response));

	board->aen_info.txptr[i].length =
	    -max(sizeof(*response)+ETH_HEADER_SIZE, ETH_MINPACKET);

	board->aen_info.txptr[i].mode = TMD1_ENP|TMD1_STP|TMD1_OWN;

	board->aen_info.tx_buffer_index = (i+1) % TXCOUNT;
    }
    /* else packet dropped with extreme prejudice */
#ifdef DEBUG
    else { printf("Ethernet packet dropped (no transfer buffers open)\n"); }
#endif /* DEBUG */

    /* it's enough that we tried */
    rkdb.last_transmit_id = response->transmit_id;
}

/*
** Wait_For_KDB_Packet:
**
** This function waits for a kernel debug packet on `device'.  device
** in an index into the aen_board array pointing to the board information
** for that device.
**
** This function is used on the debugged system to wait for the next
** kernel debug message and returns it so it can be processed.
**
** <BUG> if this code gets called from Kdebug() you must do
** a `board->aen_info.rxptr[i].mode = RMD1_OWN'?
*/
kdebug_t *
Wait_For_KDB_Packet(device)
int device;
{
    aen_board_t *board = &aen_board[device];
    unsigned short status;
    register int i;
    static kdebug_t request;

    *board->aen_info.lance_addr = RAP_CSR0; /* Status register 0 */

    while (1)
    {
	/* Wait until we get a packet */
	     
#ifdef DEBUG
	printf("Wating for a debug packet\n");
#endif /* DEBUG */
	do
	{
	    while (!((status = *board->aen_info.lance_data)&CSR0_INTR))
		;
	    *board->aen_info.lance_data = status;
	} while (!(status&CSR0_RINT));

#ifdef DEBUG
	printf("Got a packet\n");
#endif /* DEBUG */

	for (i = board->aen_info.rx_buffer_index;
	     !(board->aen_info.rxptr[i].mode&RMD1_OWN);
	     board->aen_info.rxptr[i].mode = RMD1_OWN,
	     i = (i+1) % RXCOUNT, board->aen_info.rx_buffer_index = i)
	{
	    kdebug_t *buff = (kdebug_t *)(board->aen_info.board_base +
		board->aen_info.rxptr[i].loaddr + ETH_HEADER_SIZE);
	    struct ether_header *ehead =
		(struct ether_header *)(board->aen_info.board_base +
		board->aen_info.rxptr[i].loaddr);

	    if (ehead->ether_type == ETHERTYPE_KERNEL_DEBUG &&
		buff->direction == KDB_DIR_SEND)
	    {
		bcopy((caddr_t)buff, (caddr_t)&request, sizeof(request));
		bcopy((caddr_t)ehead->ether_shost,
		      (caddr_t)rkdb.dst_addr,
		      sizeof(ether_addr_t));
		board->aen_info.rxptr[i].mode = RMD1_OWN;
		board->aen_info.rx_buffer_index = (i+1) % RXCOUNT;

#ifdef DEBUG
		printf("Returning request\n");
#endif /* DEBUG */
		return &request;
	    }
	}
    }
}
