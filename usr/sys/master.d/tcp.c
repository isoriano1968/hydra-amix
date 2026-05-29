/*#ident	"@(#)kernel:master.d/tcp	1.5"
*
*
*  		PROPRIETARY NOTICE (Combined)
*  
*  This source code is unpublished proprietary information
*  constituting, or derived under license from AT&T's Unix(r) System V.
*  In addition, portions of such source code were derived from Berkeley
*  4.3 BSD under license from the Regents of the University of
*  California.
*  
*  
*  
*  		Copyright Notice 
*  
*  Notice of copyright on this source code product does not indicate 
*  publication.
*  
*  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
*  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
*  	          All rights reserved.
*
*
*FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
fs 	-	tcp	-	-	-
*/
#define	STRNET

#ifdef INET
#include <netinet/symredef.h>
#endif /* INET */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#ifdef SYSV
#include <sys/cmn_err.h>
#endif

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#define NTCP  512
/* make TCPDEBUG same size as NTCP to enable debugging via trpt */
#define TCPDEBUG 4


struct tcp_debug tcp_debug[TCPDEBUG];
char tcp_dev[(NTCP+7)/8];
int tcp_ndebug = {TCPDEBUG};
int ntcp = {NTCP};
