#include "sys/types.h"
#include "sys/vnode.h"
#include "sys/kmacct.h"
/*#ident	"@(#)kernel:master.d/kmacct	1.1"
 *
 * Kernel Memeory Manager (KMA) Accounting
 *
 *FLAG	#VEC	PREFIX	SOFT	#DEV	IPL	DEPENDENCIES/VARIABLES
 ocs	-	kmac	-
*/
/*
 * KMARRAY is the number of entries in the symbol table
 */

#define KMARRAY  150

/*
 * SDEPTH is the depth of stack to trace back (no larger than MAXDEPTH 
 * from sys/kmacct.h)
 */

#define SDEPTH  5

/*
 * NKMABUF is the number of buffer headers to allocate (one for each
 * buffer that has been allocated and not yet returned).
 */

#define NKMABUF  1000

int nkmasym = {KMARRAY};
int kmadepth  = {SDEPTH};
int nkmabuf = {NKMABUF};
struct kmasym kmasymtab[KMARRAY];
int kmastack[KMARRAY*SDEPTH];
struct kmabuf kmabuf[NKMABUF];
