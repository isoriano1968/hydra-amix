#include "sys/types.h"
/*
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/signal.h"
#include "sys/immu.h"
#include "sys/errno.h"
#include "sys/sysmacros.h"
#include "macros.h"
#include "sys/systm.h"
*/
#include "sys/termio.h"
#include "sys/tty.h"
/*#ident	"@(#)kernel:master.d/opts	1.1"
 *
 * PTTY  - slave half driver
 *
 *FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
 ocs	-	pts	26	10	-
*/
#define	NPTTY	10	

struct tty pts_tty[NPTTY];	/* not perfect, but will work */
int pts_cnt = NPTTY;
