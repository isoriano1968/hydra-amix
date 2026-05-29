#define INITIALIZE_AEN_DEVICE
#include "sys/types.h"
#include "sys/param.h"
#include "sys/proc.h"
#include "sys/procfs.h"
#include "sys/psw.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/sysinfo.h"
#include "sys/callo.h"
#include "sys/signal.h"
#include "sys/sbd.h"
#include "sys/pcb.h"
#include "sys/stream.h"
#include "sys/kmem.h"
#include "sys/trap.h"
#include "vm/seg.h"
#include "vm/as.h"
#include "sys/user.h"
#include "sys/inline.h"
#include "sys/errno.h"
#include "aenuser.h"
#include "aen.h"
#include "kdebug.h"

/*
** Structure of a 10Mb/s Ethernet packet.
*/
struct	ethernet_packet
{
    ether_addr_t destination_address;
    ether_addr_t source_address;
    u_short	 type;
    u_char	 data[1];
};

typedef struct ethernet_packet ethernet_packet_t;

extern int kdebug();
extern int Handle_KDB_Packet();

int
kdb_trap_hook_handler(pcbp)
pcb_t *pcbp;
{
    return kdebug(pcbp, 0);
}

int
kdb_panic_hook_handler(pcbp)
pcb_t *pcbp;
{
    return kdebug(pcbp, 1);
}

void
kdb_clock_hook_handler(pcbp)
pcb_t *pcbp;
{
    (void) kdebug(pcbp, 2);
}

int
kdb_aen_packet_hook_handler(ethernet_packet, device)
ethernet_packet_t *ethernet_packet;
int device;
{
    /*
    ** If ethernet TYPE field is `ag' then we pass this along to the
    ** kernel debug code.
    **
    ** I chose `ag' without consulting the IEEE committee because I
    ** have a feeling that it isn't used and ... well ... it's my
    ** username.
    **
    ** Pax, Keith (ag@amix.commodore.com)
    */

    if (ethernet_packet->type == ETHERTYPE_KERNEL_DEBUG &&
	((kdebug_t *)ethernet_packet->data)->direction == KDB_DIR_SEND)
    {
	kdebug_t *kdbg = (kdebug_t *)ethernet_packet->data;
	int s;

#ifdef DEBUG
	printf("received a KDB packet from the aen device\n");
#endif /* DEBUG */
	
	s = splhi();

	(void)Handle_KDB_Packet(kdbg, device, ethernet_packet->source_address);
#ifdef DEBUG
	printf("handled kdb packet\n");
#endif /* DEBUG */

	splx(s);

	return 1;
    }

    return 0;
}

void
kdb_init_hooks()
{
    extern void (*clock_hook)(), aenautoconfig();
    extern int (*trap_hook)(), (*panic_hook)(), (*aen_packet_hook)();
    extern int aen_initialize();

    trap_hook=kdb_trap_hook_handler;
    panic_hook=kdb_panic_hook_handler;
    clock_hook=kdb_clock_hook_handler;
    aen_packet_hook=kdb_aen_packet_hook_handler;
#ifdef INITIALIZE_AEN_DEVICE
    aenautoconfig();

    if (!aen_initialize(0))
	printf("KDB: Couldn't initialize ethernet device\n");
#endif /* INITIALIZE_AEN_DEVICE */
}
