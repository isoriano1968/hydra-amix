/*
*#ident	"@(#)kern-port:master.d/tirdwr	10.3"
*
* TIRDWR  TLI read/write interface module
*
*FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
m	-	trw
*/
#include "sys/types.h"
#include "sys/param.h"
#include "sys/stream.h"

#define	TIRDWR_CNT	32

struct trw_trw {
	long 	 trw_flags;
	queue_t	*trw_rdq;
	mblk_t  *trw_mp;
};

struct trw_trw trw_trw[TIRDWR_CNT];
int trw_cnt = TIRDWR_CNT;
