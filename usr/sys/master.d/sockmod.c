/*
*#ident	"@(#)kernel:master.d/sockmod	1.5"
*
* SOCKMOD  Socket module
*
*FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
m	-	sock
*/
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/socketvar.h>
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/tiuser.h>
#include <sys/strlog.h>
#include <sys/signal.h>
#include <sys/sysmacros.h>
#include <sys/ticlts.h>
#include <sys/sockmod.h>

#define	SOCKMAX	200	/* just a guess */
struct so_so so_so[SOCKMAX];
int so_cnt = SOCKMAX;
