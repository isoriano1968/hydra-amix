
/*#ident	"@(#)kernel:master.d/ptm	1.1"
 * PTM   master pseudo terminal
 *
 *FLAG   #VEC    PREFIX  SOFT    #DEV    IPL     DEPENDENCIES/VARIABLES
 fs	-	ptm	-        	
*/
#include "sys/types.h"
#include "sys/param.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/errno.h"
#include "sys/user.h"
#include "sys/ptms.h"

#define	PT_CNT	32 /* ??? */

struct pt_ttys ptms_tty[PT_CNT];
int pt_cnt = PT_CNT;
