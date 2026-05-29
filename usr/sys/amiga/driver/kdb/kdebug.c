#include "sys/types.h"
#include "sys/param.h"
#include "sys/proc.h"
#include "sys/procfs.h"
#include "sys/stream.h"
#include "sys/kmem.h"
#include "sys/sysmacros.h"
#include "sys/trap.h"
#include "vm/seg.h"
#include "vm/as.h"
#include "sys/user.h"
#include "sys/sbd.h"
#include "sys/inline.h"
#include "sys/errno.h"
#include "aenuser.h"
#include "aen.h"
#include "kdebug.h"

#ifdef DEBUG
void kdbhexdump();
#endif /* DEBUG */

extern char   *vec_msg[];
extern kdebug_t *Wait_For_KDB_Packet();
int dorkdb = 1;

rkdb_t rkdb;
static void getgregs(), setgregs();

/*
** kdebug();  Run on the debugged system whenever a trap, panic, or
** clock tick occurs to handle replies to TRACE operations,
** panics or whatnot.
**
** what = 0	beginning of k_trap(); don't stop on BUS_ERROR
**	= 1	end of k_trap(); stop no matter what --PANIC.
**	= 2	clock(); don't stop unless PR_DSTOP.
*/
int
kdebug(context, camefrom)
pcb_t *context;
int camefrom;
{
    int s;
    kdebug_t *request;

    if (camefrom==2 && !(!USERMODE(GETPSW(*context)) && (rkdb.flags&PR_DSTOP)))
	return 0;		/* didn't handle--actually ret val */
				/* isn't used in clock(). */
    s = splhi();

    if (rkdb.flags&PR_DSTOP)
    {
	rkdb.flags &= ~PR_DSTOP;
	rkdb.why = PR_REQUESTED;
	rkdb.what = 0;
	rkdb.signo = 0;
	rkdb.code = 0;
    }
    else
    {
	switch(context->vector >> 2)
	{
	case BUS_ERROR:		/* kernel mode bus error */
	    if (camefrom == 0)
	    {
		splx(s);
		return 0;	/* didn't handle; let k_trap do it */
	    }
	    rkdb.why = PR_FAULTED;
	    rkdb.what = FLTBOUNDS;
	    rkdb.signo = SIGBUS;
	    rkdb.code = SEGV_MAPERR;
	    break;

	case TRACE:		/* kernel mode trace exception */
	    if (rkdb.tracing)
	    {
		rkdb.tracing = 0;
		GETPSW(*context).IPL = rkdb.ipl;
		/*
		** <IMPLEMENT>
		** Check PC here and emulate instruction if it deals
		** with the SR.
		*/
	    }

	    GETPSW(*context).TE = 0; /* clear trace enable bit */

	    rkdb.why = PR_FAULTED;
	    rkdb.what = FLTTRACE;
	    rkdb.signo = SIGTRAP;
	    rkdb.code = TRAP_TRACE;
	    break;

	case TRAP_1:
	    GETPC(*context) -= 2; /* backup pc */
	    rkdb.why = PR_FAULTED;
	    rkdb.what = FLTBPT;
	    rkdb.signo = SIGTRAP;
	    rkdb.code = TRAP_BRKPT;
	    break;

	default:
	    rkdb.why = PR_FAULTED;
	    rkdb.what = FLTILL;
	    rkdb.signo = SIGILL;
	    rkdb.code = ILL_ILLOPC;
	    break;
	}
    }

    if (camefrom == 1)
    {
	int vecnum = context->vector >> 2;
	char *vecstr;

	if (!dorkdb)
	{
	    splx(s);
	    return 0;		/* never do kdebug */
	}

	if (vecnum < 64)
	    vecstr = vec_msg[vecnum];
	else
	    vecstr = "User Defined Vector";

	printf("\nKDB Activated: %s [%d], pc=0x%x\n", vecstr, vecnum,
	       GETPC(*context));

 	if (context->format == 0xA || context->format == 0xB)


	if (dorkdb == 1)	/* do kdebug if press `ESC' */
	{
	    int debugging = 0;

	    printf("Press [Return] to enter debugger or [Esc] to PANIC.");

	    while (!debugging)
	    {
		int c = getchar();

		switch (c)
		{
		case 0x0D:
		case 0x0A:
		    debugging=1;
		    putchar('\n');
		    break;

		case 0x1B:
		    splx(s);
		    return 0;
		}
	    }
	}
    }

    rkdb.context = context;
    rkdb.flags |= PR_STOPPED|PR_ISSYS;

    if (rkdb.wstop_id)
    {
	/* send a reply. */
	kdebug_t *response = &rkdb.last_response;

	response->direction = KDB_DIR_REPLY;
	response->transmit_id = rkdb.wstop_id;
	response->kdb_error = 0;

	Send_KDB_Packet(response, rkdb.device, rkdb.dst_addr);
	rkdb.wstop_id = 0;
    }

    /*
    ** Wait for packet that asks us to continue.
    */

    do
    {
	request = Wait_For_KDB_Packet(rkdb.device);
    } while (!Handle_KDB_Packet(request, rkdb.device, rkdb.dst_addr));

    splx(s);

    return 1;			/* handled exception--continue */
}

int
Handle_KDB_Packet(request, device, src_addr)
kdebug_t *request;
int device;
ether_addr_t src_addr;
{
    int runmode = 0;
    kdebug_t *response;

#ifdef DEBUG
    kdbhexdump(request, sizeof(*request), "Handle_KDB_Packet\n");
#endif /* DEBUG */
    response = &rkdb.last_response;

    /*
    ** Reply to the packet again with the saved response
    ** if we get another packet with the same trasmit_id
    ** as the last one.
    */

    if (request->transmit_id != rkdb.last_transmit_id)
    {
	/*
	** Use last response buffer as output buffer.
	**
	** Get interesting data from incomming buffer and
	** setup ethernet frame for successful reply.
	*/

	response->direction = KDB_DIR_REPLY;
	response->transmit_id = request->transmit_id;
	response->kdb_error = 0;

	/*
	** Figure out what they want us to do ... or barf.
	*/

	rkdb.wstop_id = 0;

	switch (request->kdb_command)
	{
	case KDB_ECHO:		/* echo back a nice response. */
	    break;

	case KDB_STATUS:	/* get process status */
	{
#ifdef DEBUG
	    printf("KDB_STATUS: %s\n", (rkdb.flags&PR_STOPPED) ? "stopped":"");
#endif /* DEBUG */
	    response->kdb_status.flags = rkdb.flags;

	    if (rkdb.flags&PR_STOPPED)
	    {
		response->kdb_status.why = rkdb.why;
		response->kdb_status.what = rkdb.what;
		response->kdb_status.signo = rkdb.signo;
		response->kdb_status.code = rkdb.code;
		getgregs(response->kdb_status.reg, rkdb.context);
		response->kdb_status.instr =
		    *(long *)(response->kdb_status.reg[R_PC]);
	    }
	    break;
	}

	case KDB_READ:
	{
	    int n =
		min(sizeof(response->kdb_rdwr.data), request->kdb_rdwr.count);
	    bcopy(request->kdb_rdwr.seek, response->kdb_rdwr.data, n);
	    response->kdb_rdwr.count = n;
	    break;
	}

	case KDB_WRITE:
	    bcopy(request->kdb_rdwr.data, request->kdb_rdwr.seek,
		  request->kdb_rdwr.count);
	    response->kdb_rdwr.count = request->kdb_rdwr.count;
	    break;

	case KDB_RUN:		/* make process runnable */
	    if (!(rkdb.flags&PR_STOPPED))
	    {
		response->kdb_error = EBUSY;
		break;
	    }

	    runmode = 1;

	    if (request->kdb_run.argp)
	    {
		if (request->kdb_run.flags&PRSVADDR)
		{
		    /* Continue at address given in .vaddr */
		    GETPC(*rkdb.context) =
			(unsigned long)request->kdb_run.vaddr;
		}

		if (request->kdb_run.flags&PRSTEP)
		{
		    /* Single step */
		    GETPSW(*rkdb.context).TE = 2;

		    /*
		    ** Kludge here to handle problems with interrupts.
		    */

		    rkdb.ipl = (int)GETPSW(*rkdb.context).IPL;
		    rkdb.tracing = 1;
		    GETPSW(*rkdb.context).IPL = 7; /* high */
		    rkdb.pc = (unsigned short *)GETPC(*rkdb.context);
		}

#if HANDLINGSTEPJUMPS
		if (request->kdb_run.flags&PRSTEPJUMPS)
		{
		    /* Single step */
		    GETPSW(*rkdb.context).TE = 1;
		}
#endif
	    }
	    rkdb.flags &= ~(PR_STOPPED|PR_ISSYS);
	    break;

	case KDB_STOP:		/* post STOP request and... */
	    /* Need to post a stop request here */
	    if (!(rkdb.flags&PR_STOPPED))
		rkdb.flags |= PR_DSTOP;
	    /* Fall Through */
	case KDB_WSTOP:		/* wait for process to STOP */
	    if (!(rkdb.flags&PR_STOPPED))
	    {
		rkdb.wstop_id = request->transmit_id;
		return 0;
	    }
	    break;

	case KDB_NMAP:		/* get number of memory mappings */
	    response->kdb_nmap.num = 3;
	    break;

	case KDB_MAP:		/* get memory map information */
	    /*
	    ** Three maps:
	    ** 0 =	Text, Data, Bss, and other physmem
	    ** 1 =	u area
	    ** 2 =	kas mapped area
	    */
	    switch (request->kdb_map_request.index)
	    {
	    case 0:
		response->kdb_map_response.vaddr = (caddr_t)MAINSTORE;
		response->kdb_map_response.size = (unsigned long)VSIZOFMEM;
		response->kdb_map_response.off = (off_t)MAINSTORE;
		response->kdb_map_response.mflags = MA_READ|MA_WRITE|MA_EXEC;
		break;

	    case 1:
		response->kdb_map_response.vaddr = (caddr_t)&u;
		response->kdb_map_response.size = (unsigned long)ctob(USIZE);
		response->kdb_map_response.off = (off_t)&u;
		response->kdb_map_response.mflags = MA_READ|MA_WRITE;
		break;
		
	    case 2:
	    {
		struct seg *seg;
		u_int ls_size;
		addr_t ss_base;
		extern struct as kas;
		
		if (kas.a_segs)
		{
		    ls_size = kas.a_segs->s_size;
		    ss_base = kas.a_segs->s_base;

		    for (seg = kas.a_segs; seg != NULL; seg = seg->s_next)
		    {
			if (ls_size > kas.a_segs->s_size)
			    ls_size = kas.a_segs->s_size;

			if (ss_base < kas.a_segs->s_base)
			    ss_base = kas.a_segs->s_base;

		    }
		    response->kdb_map_response.vaddr = (caddr_t)ss_base;
		    response->kdb_map_response.off = (off_t)ss_base;
		    response->kdb_map_response.size = (unsigned long)ls_size;
		    response->kdb_map_response.mflags =
			MA_READ|MA_WRITE|MA_EXEC;
		}
		break;
	    }

	    default:
		response->kdb_error = EINVAL;
	    }
	    break;

	case KDB_GREG:		/* get general registers */
	    getgregs(response->kdb_gregs, rkdb.context);
	    break;

	case KDB_SREG:		/* set general registers */
	    setgregs(rkdb.context, request->kdb_gregs);
	    break;

	case KDB_GFPREG:	/* get floating-point registers */
	    break;

	case KDB_SFPREG:	/* set floating-point registers */
	    break;

	case KDB_GFAULT:	/* get traced fault set */
	case KDB_SFAULT:	/* set traced fault set */
	case KDB_CFAULT:	/* clear current fault */
	    response->kdb_error = EPROTONOSUPPORT;
	    break;

	default:
#ifdef DEBUG
	    printf("bad command 0x%x\n", request->kdb_command);
#endif /* DEBUG */
	    response->kdb_error = EINVAL;
	    break;
	}
    }

    Send_KDB_Packet(response, device, src_addr);

    return runmode;
}

static void
getgregs(reg, context)
     register gregset_t reg;
     register pcb_t *context;
{
    reg[R_D0] = context->regsave[K_D0];
    reg[R_D1] = context->regsave[K_D1];
    reg[R_D2] = context->regsave[K_D2];
    reg[R_D3] = context->regsave[K_D3];
    reg[R_D4] = context->regsave[K_D4];
    reg[R_D5] = context->regsave[K_D5];
    reg[R_D6] = context->regsave[K_D6];
    reg[R_D7] = context->regsave[K_D7];
    reg[R_A0] = context->regsave[K_A0];
    reg[R_A1] = context->regsave[K_A1];
    reg[R_A2] = context->regsave[K_A2];
    reg[R_A3] = context->regsave[K_A3];
    reg[R_A4] = context->regsave[K_A4];
    reg[R_A5] = context->regsave[K_A5];
    reg[R_A6] = context->regsave[K_A6];
    reg[R_A7] = context->regsave[K_A7];
    reg[R_PS] = (greg_t)*(short int *)&context->psw;
    reg[R_PC] = (greg_t)GETPC(*context);
}

static void
setgregs(context, reg)
     register pcb_t *context;
     register gregset_t reg;
{
    context->regsave[K_D0] = reg[R_D0];
    context->regsave[K_D1] = reg[R_D1];
    context->regsave[K_D2] = reg[R_D2];
    context->regsave[K_D3] = reg[R_D3];
    context->regsave[K_D4] = reg[R_D4];
    context->regsave[K_D5] = reg[R_D5];
    context->regsave[K_D6] = reg[R_D6];
    context->regsave[K_D7] = reg[R_D7];
    context->regsave[K_A0] = reg[R_A0];
    context->regsave[K_A1] = reg[R_A1];
    context->regsave[K_A2] = reg[R_A2];
    context->regsave[K_A3] = reg[R_A3];
    context->regsave[K_A4] = reg[R_A4];
    context->regsave[K_A5] = reg[R_A5];
    context->regsave[K_A6] = reg[R_A6];
    context->regsave[K_A7] = reg[R_A7];
/*  (greg_t)*(short int *)&context->psw = reg[R_PS];  *****WRONG*****/
    GETPC(*context) = reg[R_PC];
}

#ifdef DEBUG
void
kdbhexdump(buf, length, header)
char *buf, *header;
int length;
{
    int i;
    unsigned int hd_address = 0;
    char hd_text[17], *hdp, hd_addrtext[4], *hda;
    static char hd_spaces[]="                                                ";

    printf("%s", header);

    while (length)
    {
	hda = hd_addrtext;

	if (hd_address < 0x1000)
	    *hda++ = '0';

	if (hd_address < 0x100)
	    *hda++ = '0';

	if (hd_address < 0x10)
	    *hda++ = '0';

	*hda = '\0';
	printf("%s%x: ", hd_addrtext, hd_address);

	hdp = hd_text;
	for (i=0; i < 16 && length; ++i, --length, ++hd_address)
	{
	    register unsigned char c = *buf++;

	    *hdp++ = ((c < 20) || ((c > 0x7F) && (c < 0xa0))) ? '.' : c;

	    if (c < 0x10)
		printf("0%x ", c);
	    else
		printf("%x ", c);
	}

	if (i < 16)
	    printf("%s", &hd_spaces[(16*3) - ((16-i)*3)]);

	*hdp = '\0';
	printf("%s\n", hd_text);
    }
}
#endif /* DEBUG */
