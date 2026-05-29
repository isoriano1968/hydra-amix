#include	"sys/types.h"
#include	"sys/param.h"
#include	"sys/errno.h"
#include	"sys/kmem.h"
#include	"sys/inline.h"
#include	"rico.h"
#include	"sd.h"


#define	DMA_GRANULARITY	4


static bool	done;

void	ihandle( );
paddr_t	vtop( );


gsioctl( dev, cmd, args)
{
	static struct sdcom	sdcom;
	static uchar		iobuf[1024];
	uint			ua;
	int			error;

	unless (cmd == 'GSIO')
		return (EINVAL);
	if (copyin( args, &sdcom, sizeof sdcom) < 0)
		return (EFAULT);
	if (error=sdopen( sdcom.card))
		return (error);
	if ((sdcom.nbyte % DMA_GRANULARITY)
	or (sdcom.nbyte > sizeof iobuf))
		return (EINVAL);
	if ((sdcom.nbyte)
	and (not sdcom.reading)
	and (copyin( sdcom.addr, iobuf, sdcom.nbyte) < 0))
		return (EFAULT);
	ua = sdcom.addr;
	sdcom.addr = iobuf;
	sdcom.intr = ihandle;
	done = FALSE;
	sdqueue( &sdcom);
	sdspl( );
	until (done)
		sleep( &sdcom, PRIBIO);
	spl0( );
	if ((sdcom.nbyte)
	and (sdcom.okay)
	and (sdcom.reading)
	and (copyout( iobuf, ua, sdcom.nbyte) < 0))
		return (EFAULT);
	if (copyout( &sdcom, args, sizeof sdcom) < 0)
		return (EFAULT);
	return (0);
}


static void
ihandle( cp)
struct sdcom	*cp;
{

	done = TRUE;
	wakeup( cp);
}
