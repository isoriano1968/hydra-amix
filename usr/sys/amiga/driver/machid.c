/*
 * A driver which returns information about "machine identification".
 * Used by the install script to decide whether to install an A2500
 * or A3000 kernel.
 */

#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/errno.h"
#include "sys/buf.h"
#include "sys/systm.h"
#include "sys/inline.h"


extern unsigned long boot_arg0;


static void puthex(p, n)
char *p;
unsigned long n;
{
	int i;

	for ( i=0 ; i<32 ; i+=4 )
		*p++ = "0123456789abcdef"[(n>>(28-i))&0xf];
	*p = 0;
}


/*
 * Search for memory below MAINSTORE that could be used if an
 * appropriate kernel were loaded.
 */
static unsigned long memstart()
{
	volatile unsigned long *p;
	volatile unsigned long *mp = (volatile unsigned long *)MAINSTORE;
	unsigned long start, save;

	start = MAINSTORE;
	save = *mp;

	while (start >= 0x07000000 &&
	       start <= 0x08000000) {
		/* Go down 4 meg at a time. */
		p = (volatile unsigned long *)(start - 0x00400000);
		*p = 0;
		if (*p != 0 ||
		    *mp != save)
			break;

		*p = 0x01234567;
		if (*p != 0x01234567 ||
		    *mp != save)
			break;

		*p = 0xfedcba98;
		if (*p != 0xfedcba98 ||
		    *mp != save)
			break;

		start = (unsigned long)p;
	}

	*mp = save;
	return start;
}


/*
 * Read the machine id.  For now, it returns a newline-terminated
 * ASCII string like "A2500 1 00200000" or "A3000 2 07800000".  First
 * token is machine type, second is boot method (1==A2620, 2==EXEC0),
 * third is beginning of primary physical memory.
 */
int miread(dev, uiop, cr)
dev_t dev;
struct uio *uiop;
struct cred *cr;
{
	int error;
	static char idbuf[20], flag;

	if (!flag) {
		char *p = idbuf;
		flag = 1;
		strcpy(p, (((unsigned long)miread)<0x1000000) ?
			      "A2500" : "A3000");
		p += strlen(p);
		*p++ = ' ';
		*p++ = '0' + boot_arg0;
		*p++ = ' ';
		puthex(p, memstart());
		p += strlen(p);
		*p++ = '\n';
		*p++ = 0;
	}

	if (uiop->uio_offset == 0)
		return uiomove(idbuf, strlen(idbuf), UIO_READ, uiop);
	else
		return 0;
}
