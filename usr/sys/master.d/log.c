#include "sys/types.h"
#include "sys/stream.h"
#include "sys/log.h"
/*
*#ident	"@(#)kern-port:master.d/log	10.3"
*
* LOG	Streams Log Driver
*
*FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
orfsn	-	log	-	-	-
*/
#define NLOG	16

int log_cnt = NLOG+6;
struct log log_log[NLOG+6];
