/*#ident	"@(#)kernel:master.d/ptem	1.3"
 *
 * PTEM - Pseudo-TTY streams module
 *
 *FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
 m	-	ptem	-	-	-
*/
#include "sys/types.h"
#include "sys/param.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/termio.h"
#include "sys/user.h"
#include "sys/strtty.h"
#include "sys/errno.h"
#include "sys/jioctl.h"
#include "sys/ptem.h"

#define	PTEM_CNT 32	/* ??? */

struct ptem ptem[PTEM_CNT];
int ptem_cnt = PTEM_CNT;
