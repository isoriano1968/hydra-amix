

/*
 * SCSI disk partition
 *
 * This module provides partition services as given by the Rigid Disk Block.
 * Non-SCSI devices are not supported.  Minor device bits are defined by sd.h.
 * Partition 0 always refers to the entire unmapped device.
 */

#include	"sys/types.h"
#include	"sys/sysmacros.h"
#include	"sys/param.h"
#include	"sys/buf.h"
#include	"sys/conf.h"
#include	"sys/signal.h"
#include	"sys/dir.h"
#include	"sys/user.h"
#include	"sys/errno.h"
#include	"sys/inline.h"
#include	"rico.h"
#include	"sd.h"
#include	"rdb.h"

#ifndef BSIZE
#define BSIZE 512
#endif


struct partab {
	ulong	base,
		len;
};
union block {
	struct RigidDiskBlock	rdb;
	struct PartitionBlock	pb;
	char			bytes[BSIZE];
};


static struct partab	partab[0x100];
static union block	block;

int	getrdb( ),
	getpb( ),
	read( );
bool	checksum( );


int
sdpartition( dev, strat)
int	(*strat)( );
{
	ulong	a,
		sperc;
	uint	pa;
	int	error;

	if (not sdpart( dev))
		return 0;
	if (error = getrdb( dev, strat))
		return error;
	sperc = block.rdb.rdb_CylBlocks;
	a = block.rdb.rdb_PartitionList;
	pa = 0;
	do {
		if (a == -1)
			return ENXIO;
		if (error = getpb( dev, strat, a))
			return error;
		a = block.pb.pb_Next;
	} while (++pa < sdpart( dev));
	partab[MINOR( dev)].base = block.pb.pb_Environment[DE_LOWCYL] * sperc;
	partab[MINOR( dev)].len = (block.pb.pb_Environment[DE_UPPERCYL]
	                       +1-block.pb.pb_Environment[DE_LOWCYL]) * sperc;
	return 0;
}


bool
sdvalid( bp)
struct buf	*bp;
{

	if ((not sdpart( bp->b_edev))
	or (bp->b_blkno + (bp->b_bcount+BSIZE-1)/BSIZE <= partab[MINOR( bp->b_edev)].len))
		return (TRUE);
	bp->b_resid = bp->b_bcount;
	bp->b_flags |= B_ERROR;
	iodone( bp);
	return (FALSE);
}


uint
sdblkno( bp)
struct buf	*bp;
{

	return (partab[MINOR( bp->b_edev)].base + bp->b_blkno);
}


/* Returns size of a partition, or a large number for a device which has never been opened. */
uint
sddevsize( dev)
dev_t	dev;
{

	return partab[MINOR( dev)].len ? partab[MINOR( dev)].len : 1234567;
}


static int
getrdb( dev, strat)
int	(*strat)( );
{
	ulong	i;
	int	error;

	for (i=0; i<16; ++i) {
		if (error = read( dev, strat, i))
			return error;
		if ((block.rdb.rdb_ID == 'RDSK')
		and (checksum( &block.rdb, block.rdb.rdb_SummedLongs)))
			return 0;
	}
	return ENXIO;
}


static int
getpb( dev, strat, a)
int	(*strat)( );
ulong	a;
{
	int 	error;

	if (error = read( dev, strat, a))
		return error;
	if ((block.pb.pb_ID == 'PART')
	and (checksum( &block.pb, block.pb.pb_SummedLongs)))
		return 0;
	return ENXIO;
}


static int
read( dev, strat, bno)
ulong	bno;
int	(*strat)( );
{
	static struct buf	b;
	static bool		busy;
	int			x;

	while (busy)
		sleep( &busy, PRIBIO);
	busy = TRUE;
	b.b_bcount = sizeof block;
	b.b_blkno = bno;
	b.b_edev = sddev0p( dev);
	b.b_flags = B_READ|B_KERNBUF;
	b.b_un.b_addr = &block;
	b.b_error = 0;
	(*strat)( &b);
	x = sdspl( );
	while (not (b.b_flags&B_DONE))
		sleep( &b, PRIBIO);
	splx( x);
	busy = FALSE;
	wakeup( &busy);
	if ((b.b_flags & B_ERROR)
	or (b.b_resid))
		return b.b_error ? b.b_error : EIO;
	return 0;
}


static bool
checksum( p, n)
ulong	*p,
	n;
{
	ulong	a;

	a = 0;
	while (n--)
		a += *p++;
	return (a == 0);
}
