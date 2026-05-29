/*
*#ident	"@(#)kern-port:master.d/sp	10.3"
* SP - streams pipe driver
*
*FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
fs	-	sp	-	
*/
#define	SP_CNT	100	/* ??? */

char sp_sp[SP_CNT*8];
int spcnt = SP_CNT;


