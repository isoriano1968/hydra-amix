/*#ident	"@(#)kernel:master.d/ip	1.4"
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
fs 	-	ip	-	-	-
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strlog.h>
#include <sys/log.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <net/strioc.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/route.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_str.h>

#define IPCNT 8
#define IPPROVCNT 16
/* if IPFORWARDING is set, hosts will act as gateways */
#define IPFORWARDING 1
/* if IPSENDREDIRECTS is set, gateways may send ICMP redirects */
#define IPSENDREDIRECTS 1

struct ip_pcb ip_pcb[IPCNT];
struct ip_provider provider[IPPROVCNT];
int ipcnt = {IPCNT};
int ipprovcnt = {IPPROVCNT};
int ipforwarding = {IPFORWARDING};
int ipsendredirects = {IPSENDREDIRECTS};
