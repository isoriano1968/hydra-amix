/*#ident	"@(#)kernel:master.d/arp	1.1"
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
*fs 	-	arp	-	-	-
* ARPTAB_SIZE = (ARPTAB_BSIZ * ARPTAB_NB)
*/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/log.h>
#include <sys/strlog.h>
#include <sys/dlpi.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/strioc.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>

#define ARPTAB_BSIZ  9
#define ARPTAB_NB  19
#define ARPTAB_SIZE  171

struct arptab arptab[ARPTAB_SIZE];
int arptab_bsiz = {ARPTAB_BSIZ};
int arptab_nb = {ARPTAB_NB};
int arptab_size = {ARPTAB_SIZE};
