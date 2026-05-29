

/*
 * QIC cartridge tape drive, SCSI controller.
 *	For use with Caliper, Sankyo, Wangtek.
 *
 * ENOSPC is returned at end-of-tape.  EIO is returned by Media Error, but
 * read resync is possible.  New Sankyo CP-150SE supported.
 */

#include	"sys/types.h"
#include	"sys/param.h"
#include	"sys/buf.h"
#include	"sys/inline.h"
#include	"sys/errno.h"
#include	"sys/file.h"
#include	"sys/uio.h"
#include	"rico.h"
#include	"sd.h"


/*
 * minor device options
 * Must be coordinated with `sdunit' and `sdcard' macros.
 */
#define	ctrewind( d)	(not ((d) & 1<<7))
#define	ctappend( d)	((d) & 1<<6)
#define	ctden150( d)	((d) & 1<<5)


struct ct {
	uint		state;
	bool		open;
	struct buf	*bhead,
			*btail;
	struct sdcom	com;
	uchar		sense[14];
};
/* ct.state
 */
#define	NORMAL_IDLE		0
#define	NORMAL_READ		1
#define	NORMAL_SPECIAL		2
#define	NORMAL_SEOF		3
#define	NORMAL_READ_GETSENSE	4
#define	NORMAL_SEOF_GETSENSE	5
#define	EOD_IDLE		6
#define	EOD_WRITE		7
#define	EOD_WRITE_GETSENSE	8
#define	EOD_SPECIAL		9
#define	EOM_IDLE		10
#define	EOM_SPECIAL		11
#define	EOF_IDLE		12
#define	EOF_SPECIAL		13
#define	DOOM_IDLE		14
#define	DOOM_SPECIAL		15
#define	DOOM_MISC_GETSENSE	16
#define	BAD_READ_1		17
#define	BAD_READ_2		18
#define	BAD_READ_NULS		19

struct special {
	bool		active;
	uchar		com,
			sense[sizeof( ((struct ct *)0)->sense)];
	struct buf	buf;
};
/* special.com
 */
#define	INIT120		0
#define	INIT150		1
#define	REWIND		2
#define	GETSENSE	3
#define	APPEND		4
#define	WEOF		5
#define	FLUSH		6
#define	SEOF		7
#define	IO		8
#define	CLOSE		9


#define	BSIZE	512


static struct ct	cttab[SDCARDS][SDUNITS];
static struct special	special;

void	breakup( ),
	strategy( );


ctopen( devp, flags)
dev_t	*devp;
{
	struct ct	*dp;
	uchar		sense2,
			sense8,
			sense9;
	int		e;

	dp = &cttab[sdcard( *devp)][sdunit( *devp)];
	if (dp->open)
		return (EBUSY);
	if ((e = sdopen( sdcard( *devp)))
	or (e = command( dp, GETSENSE)))
		return (e);
	sense2 = dp->sense[2];
	sense8 = dp->sense[8];
	sense9 = dp->sense[9];
	if (sense8 & 1<<6)
		return (ENXIO);
	if (sense9 & 1<<3) {
		dp->state = NORMAL_IDLE;
		if (e = init( *devp))
			return (e);
	}
	switch (flags & (FREAD|FWRITE)) {
	case FREAD:
		if (ctappend( *devp))
			return (EINVAL);
		break;
	case FWRITE:
		if (sense8 & 1<<4)
			return (EROFS);
		if ((flags & FAPPEND)
		or (ctappend( *devp)))
			if (e = command( dp, APPEND))
				return (e);
		unless ((sense9 & 1<<3)
		or (dp->state == EOD_IDLE))
			return (ENXIO);
		break;
	default:
		return (EINVAL);
	}
	dp->open = TRUE;
	return (0);
}


ctclose( dev, flags)
dev_t	dev;
{
	struct ct	*dp;

	dp = &cttab[sdcard( dev)][sdunit( dev)];
	if (ctrewind( dev)) {
		if (flags & FWRITE)
			command( dp, WEOF);
		command( dp, REWIND);
	}
	else
		if (flags & FWRITE)
			command( dp, WEOF);
		else
			command( dp, SEOF);
	command( dp, CLOSE);
	dp->open = FALSE;
	return (0);
}


ctread( dev, uiop)
dev_t		dev;
struct uio	*uiop;
{

	return (uiophysio( breakup, (struct buf *)0, dev, B_READ, uiop));
}


ctwrite( dev, uiop)
dev_t		dev;
struct uio	*uiop;
{

	return (uiophysio( breakup, (struct buf *)0, dev, B_WRITE, uiop));
}


static void
breakup( bp)
struct buf	*bp;
{

	amiga_dma_pageio( strategy, bp);
}


static void
strategy( bp)
struct buf	*bp;
{
	int	x;

	bp->b_resid = 0;
	x = sdspl( );
	queue( &cttab[sdcard( bp->b_edev)][sdunit( bp->b_edev)], bp);
	splx( x);
}


static
init( dev)
{

	int com = ctden150( dev)? INIT150: INIT120;
	return (command( &cttab[sdcard( dev)][sdunit( dev)], com)? ENXIO: 0);
}


static
command( dp, com)
struct ct	*dp;
{
	static bool	locked;
	int		x;

	x = sdspl( );
	while (locked)
		sleep( &locked, PZERO);
	locked = TRUE;
	special.com = com;
	special.buf.b_flags = 0;
	special.active = TRUE;
	queue( dp, &special.buf);
	while (special.active)
		sleep( &special.active, PZERO);
	splx( x);
	locked = FALSE;
	wakeup( &locked);
	if (special.buf.b_flags & B_ERROR)
		return (special.buf.b_error? special.buf.b_error: EIO);
	return (0);
}


static
queue( dp, bp)
struct ct	*dp;
struct buf	*bp;
{

	bp->av_forw = 0;
	if (dp->bhead) {
		dp->btail->av_forw = bp;
		dp->btail = bp;
	}
	else {
		dp->bhead = bp;
		dp->btail = bp;
		machine( dp);
	}
}


static
ihandle( cp)
struct sdcom	*cp;
{

	machine( &cttab[cp->card][cp->unit]);
}


static
machine( dp)
struct ct	*dp;
{
	struct buf	*bp;

	while (bp = dp->bhead)
		switch (dp->state) {
		case NORMAL_IDLE:
			unless (bp == &special.buf) {
				if (bp->b_flags & B_READ)
					start( dp, IO, NORMAL_READ);
				else
					start( dp, IO, EOD_WRITE);
				return;
			}
			switch (special.com) {
			case INIT120:
			case INIT150:
			case GETSENSE:
			case WEOF:
				start( dp, special.com, NORMAL_SPECIAL);
				return;
			case REWIND:
				start( dp, special.com, DOOM_SPECIAL);
				return;
			case APPEND:
				start( dp, special.com, EOD_SPECIAL);
				return;
			case SEOF:
				start( dp, special.com, NORMAL_SEOF);
				return;
			default:
				dispose( dp, NORMAL_IDLE);
			}
			break;
		case NORMAL_READ:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, NORMAL_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, NORMAL_READ_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case NORMAL_READ_GETSENSE:
			if ((not dp->com.okay)
			or (dp->com.status != 0)) {
				dp->state = DOOM_MISC_GETSENSE;
				break;
			}
			switch (dp->sense[2] & 0x0F) {
			case 0:
				if (setresidue( dp->sense, bp) < bp->b_bcount)
					dispose( dp, EOF_IDLE);
				dp->state = EOF_IDLE;
				break;
			case 8:
				report( dp);
				if (setresidue( dp->sense, bp) < bp->b_bcount)
					dispose( dp, EOD_IDLE);
				dp->state = EOD_IDLE;
				break;
			case 3:
				report( dp);
				if (setresidue( dp->sense, bp) < bp->b_bcount)
					dispose( dp, BAD_READ_1);
				dp->state = BAD_READ_1;
				break;
			default:
				report( dp);
				dp->state = DOOM_IDLE;
			}
			break;
		case BAD_READ_1:
			if (bp == &special.buf)
				dp->state = NORMAL_IDLE;
			else {
				bp->b_flags |= B_ERROR;
				dispose( dp, BAD_READ_2);
			}
			break;
		case BAD_READ_2:
			if (bp == &special.buf)
				dp->state = NORMAL_IDLE;
			else {
				bp->b_flags |= B_ERROR;
				dispose( dp, BAD_READ_NULS);
			}
			break;
		case BAD_READ_NULS:
			unless (bp == &special.buf) {
				bzero( vtop( paddr( bp), bp->b_proc), BSIZE);
				bp->b_resid = bp->b_bcount - BSIZE;
				dispose( dp, NORMAL_IDLE);
			}
			dp->state = NORMAL_IDLE;
			break;
		case NORMAL_SPECIAL:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, NORMAL_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, DOOM_MISC_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case NORMAL_SEOF:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, EOF_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, NORMAL_SEOF_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case NORMAL_SEOF_GETSENSE:
			if ((dp->com.okay)
			and (dp->com.status == 0)
			and ((dp->sense[2]&0x0F) == 8)) {
				report( dp);
				dispose( dp, EOD_IDLE);
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case EOD_IDLE:
			unless (bp == &special.buf) {
				if (bp->b_flags & B_READ) {
					bp->b_error = ENOSPC;
					bp->b_flags |= B_ERROR;
					dispose( dp, EOD_IDLE);
					break;
				}
				start( dp, IO, EOD_WRITE);
				return;
			}
			switch (special.com) {
			case REWIND:
				start( dp, special.com, DOOM_SPECIAL);
				return;
			case GETSENSE:
			case WEOF:
				start( dp, special.com, EOD_SPECIAL);
				return;
			default:
				dispose( dp, EOD_IDLE);
			}
			break;
		case EOD_WRITE:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, EOD_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, EOD_WRITE_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case EOD_WRITE_GETSENSE:
			if ((not dp->com.okay)
			or (dp->com.status != 0)) {
				dp->state = DOOM_MISC_GETSENSE;
				break;
			}
			switch (dp->sense[2] & 0xF) {
			case 0x0:
			case 0xD:
				report( dp);
				if (setresidue( dp->sense, bp) < bp->b_bcount)
					dispose( dp, EOM_IDLE);
				dp->state = EOM_IDLE;
				break;
			default:
				report( dp);
				dp->state = DOOM_IDLE;
			}
			break;
		case EOD_SPECIAL:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, EOD_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, DOOM_MISC_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case EOM_IDLE:
			unless (bp == &special.buf) {
				bp->b_error = ENOSPC;
				bp->b_flags |= B_ERROR;
				dispose( dp, EOM_IDLE);
				break;
			}
			switch (special.com) {
			case REWIND:
				start( dp, special.com, DOOM_SPECIAL);
				return;
			case GETSENSE:
				start( dp, special.com, EOM_SPECIAL);
				return;
			default:
				dispose( dp, EOM_IDLE);
			}
			break;
		case EOM_SPECIAL:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, EOM_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, DOOM_MISC_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case EOF_IDLE:
			unless (bp == &special.buf) {
				bp->b_resid = bp->b_bcount;
				dispose( dp, EOF_IDLE);
				break;
			}
			switch (special.com) {
			case REWIND:
				start( dp, special.com, DOOM_SPECIAL);
				return;
			case GETSENSE:
				start( dp, special.com, EOF_SPECIAL);
				return;
			case CLOSE:
				dispose( dp, NORMAL_IDLE);
				break;
			default:
				dispose( dp, EOF_IDLE);
			}
			break;
		case EOF_SPECIAL:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, EOF_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, DOOM_MISC_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case DOOM_IDLE:
			unless (bp == &special.buf) {
				bp->b_flags |= B_ERROR;
				dispose( dp, DOOM_IDLE);
				break;
			}
			switch (special.com) {
			case REWIND:
			case GETSENSE:
				start( dp, special.com, DOOM_SPECIAL);
				return;
			default:
				bp->b_flags |= B_ERROR;
				dispose( dp, DOOM_IDLE);
			}
			break;
		case DOOM_SPECIAL:
			unless (dp->com.okay)
				dp->state = DOOM_MISC_GETSENSE;
			else if (dp->com.status == 0)
				dispose( dp, DOOM_IDLE);
			else if (dp->com.status == 2) {
				start( dp, GETSENSE, DOOM_MISC_GETSENSE);
				return;
			}
			else
				dp->state = DOOM_MISC_GETSENSE;
			break;
		case DOOM_MISC_GETSENSE:
			report( dp);
			bp->b_flags |= B_ERROR;
			dispose( dp, DOOM_IDLE);
		}
}


static
dispose( dp, st)
struct ct	*dp;
{
	struct buf	*bp;

	bp = dp->bhead;
	dp->bhead = bp->av_forw;
	if (bp == &special.buf) {
		bcopy( dp->sense, special.sense, sizeof special.sense);
		special.active = FALSE;
		wakeup( &special.active);
	}
	else
		iodone( bp);
	dp->state = st;
}


static
start( dp, com, st)
struct ct	*dp;
{
	static uchar init120[] = {
		0x00, 0x00, 0x10, 0x08,
		0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
	};
	static uchar init150[] = {
		0x00, 0x00, 0x10, 0x08,
		0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00
	};
	struct buf	*bp;

	switch (com) {
	case APPEND:
		dp->com.cdb[0] = 0x11;
		dp->com.cdb[1] = 0x03;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 0;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
		break;
	case GETSENSE:
		dp->com.addr = dp->sense;
		dp->com.nbyte = sizeof dp->sense;
		dp->com.cdb[0] = 0x03;
		dp->com.cdb[1] = 0;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = sizeof dp->sense;
		dp->com.cdb[5] = 0;
		dp->com.reading = TRUE;
		break;
	case INIT120:
		dp->com.addr = init120;
		dp->com.nbyte = sizeof init120;
		dp->com.cdb[0] = 0x15;
		dp->com.cdb[1] = 0;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 0x0C;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
		break;
	case INIT150:
		dp->com.addr = init150;
		dp->com.nbyte = sizeof init150;
		dp->com.cdb[0] = 0x15;
		dp->com.cdb[1] = 0;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 0x0C;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
		break;
	case IO:
		bp = dp->bhead;
		dp->com.addr = vtop( paddr( bp), bp->b_flags&B_KERNBUF? (struct proc *)0: bp->b_proc);
		dp->com.nbyte = bp->b_bcount;
		if (bp->b_flags & B_READ) {
			dp->com.reading = TRUE;
			dp->com.cdb[0] = 0x08;
		}
		else {
			dp->com.reading = FALSE;
			dp->com.cdb[0] = 0x0A;
		}
		dp->com.cdb[1] = 0x01;
		dp->com.cdb[2] = byte2( bp->b_bcount/BSIZE);
		dp->com.cdb[3] = byte1( bp->b_bcount/BSIZE);
		dp->com.cdb[4] = byte0( bp->b_bcount/BSIZE);
		dp->com.cdb[5] = 0;
		break;
	case REWIND:
		dp->com.cdb[0] = 0x01;
		dp->com.cdb[1] = 0;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 0;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
		break;
	case SEOF:
		dp->com.cdb[0] = 0x11;
		dp->com.cdb[1] = 0x01;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 1;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
		break;
	case WEOF:
		dp->com.cdb[0] = 0x10;
		dp->com.cdb[1] = 0x01;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 1;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
		break;
	case FLUSH:
		dp->com.cdb[0] = 0x10;
		dp->com.cdb[1] = 0;
		dp->com.cdb[2] = 0;
		dp->com.cdb[3] = 0;
		dp->com.cdb[4] = 0;
		dp->com.cdb[5] = 0;
		dp->com.reading = FALSE;
	}
	dp->com.card = sdcard( dp-cttab[0]);
	dp->com.unit = sdunit( dp-cttab[sdcard( dp-cttab[0])]);
	dp->com.intr = ihandle;
	dp->state = st;
	sdqueue( &dp->com);
}


static
setresidue( sense, bp)
uchar		sense[];
struct buf	*bp;
{

	if (sense[0] == 0xF0) {
		uint r = *(ulong *)&sense[3] * BSIZE;
		if (r <= bp->b_bcount)
			bp->b_resid = r;
	}
	return (bp->b_resid);
}


static
report( dp)
struct ct	*dp;
{
	static uchar *sense[] = {
		0,
		0,
		"not ready",
		"media error",
		"hardware error",
		"illegal request",
		"unit attention",
		"data protect",
		"blank check",
		0,
		0,
		"aborted command",
		0,
		"volume overflow"
	};
	uint	s;

	printf( "%s: tape ct%d: ", sdhardwarename( dp->com.card), dp->com.card*SDUNITS+dp->com.unit);
	unless (dp->com.okay)
		printf( "SCSI timeout\n");
	else
		switch (dp->com.status) {
		case 0:
			s = dp->sense[2] & 0x0F;
			if ((s < nel( sense))
			and (sense[s]))
				printf( "%s (", sense[s]);
			else
				printf( "sense %d (", s);
			printbits( dp->sense[2], "FM EOMILI");
			printbits( dp->sense[8], "   CNI   WRPeomUDEBNLFIL");
			printbits( dp->sense[9], "      NDT   BOMBPE");
			printf( "%d)\n", *(ushort *)(dp->sense+10));
			break;
		case 8:
			printf( "busy\n");
			break;
		default:
			printf( "unknown error\n");
		}
}


static
printbits( bits, names)
uchar	*names;
{

	while (*names) {
		if (bits & 1<<7) {
			putchar( names[0]);
			putchar( names[1]);
			putchar( names[2]);
			putchar( ' ');
		}
		bits <<= 1;
		names += 3;
	}
}
