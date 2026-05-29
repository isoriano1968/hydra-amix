/*
*#ident	"@(#)kern-port:master.d/timod	10.3"
*
* TIMOD  TLI cooperating module
*
*FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
m	-	tim
*/
#include "sys/param.h"
#include "sys/types.h"
#include "sys/stream.h"

#define	TIMOD_CNT 128

struct tim_tim {
	long 	 tim_flags;
	queue_t	*tim_rdq;
	mblk_t  *tim_iocsave;
	int	 tim_mymaxlen;
	int	 tim_mylen;
	caddr_t	 tim_myname;
	int	 tim_peermaxlen;
	int	 tim_peerlen;
	caddr_t	 tim_peername;
	mblk_t	*tim_consave;
};

struct tim_tim tim_tim[TIMOD_CNT];
int tim_cnt = TIMOD_CNT;
