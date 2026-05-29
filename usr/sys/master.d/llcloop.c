/*#ident	"@(#)kernel:master.d/llcloop	1.2"
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
*fs 	-	loop	-	-	-
*/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/user.h>		/* XXX - TOM */
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strlog.h>
#include <sys/log.h>
#include <netinet/nihdr.h>
#include <sys/dlpi.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/strioc.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/llcloop.h>

#define LOOPCNT  8

struct loop_pcb loop_pcb[LOOPCNT];
struct ifstats loopstats[LOOPCNT];
int loopcnt = {LOOPCNT};
