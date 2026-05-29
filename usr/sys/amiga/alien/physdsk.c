/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 
 * Amiga UNIX SCSI Driver DMA break-up routine
 * (used by character interfaces for block devices).
 * Derived from "io/physdsk.c".
 * Someday will replace "io/physdsk.c".
 *
 */

#include	"sys/types.h"
#include	"sys/param.h"
#include	"sys/sysmacros.h"
#include	"sys/inline.h"
#include	"sys/buf.h"
#include	"sys/errno.h"
#include	"sys/immu.h"
#include	"rico.h"


struct buf	*ngeteblk( );


/*
 * DMA breakup routine.
 * Break up the request that came from physio into chunks of
 * contiguous memory.
 */
void
amiga_dma_pageio( strat, bp)
void		(*strat)( );
struct buf	*bp;
{
	struct buf	*nbp;
	int		cc;

	bp->b_resid = bp->b_bcount;
	unless (nbp = ngeteblk( NBPP)) {
		bp->b_flags |= B_ERROR | B_DONE;
		bp->b_error = EAGAIN;
		return;
	}
	nbp->b_edev = bp->b_edev;
	nbp->b_flags = bp->b_flags | B_KERNBUF;
	nbp->b_proc = 0;

	while (cc = min( bp->b_resid, NBPP)) {
		nbp->b_bcount = cc;
		nbp->b_blkno = bp->b_blkno;
		nbp->b_flags &= ~B_DONE;
		if (bp->b_flags & B_READ) {
			(*strat)( nbp);
			spl6( );
			until (nbp->b_flags & B_DONE) {
				nbp->b_flags |= B_WANTED;
				sleep( nbp, PRIBIO);
			}
			spl0( );
			if (nbp->b_flags & B_ERROR) {
				if (bp->b_resid == bp->b_bcount) {
					bp->b_error = nbp->b_error;
					bp->b_flags |= B_ERROR;
				}
				break;
			}
			unless (cc -= nbp->b_resid)
				break;
			if (bp->b_flags & B_KERNBUF)
				bcopy( nbp->b_un.b_addr, bp->b_un.b_addr, cc);
			else if (copyout( nbp->b_un.b_addr, bp->b_un.b_addr, cc)) {
				bp->b_error = EFAULT;
				bp->b_flags |= B_ERROR;
				break;
			}
		}
		else {
			if (bp->b_flags & B_KERNBUF)
				bcopy( bp->b_un.b_addr, nbp->b_un.b_addr, cc);
			else if (copyin( bp->b_un.b_addr, nbp->b_un.b_addr, cc)) {
				bp->b_error = EFAULT;
				bp->b_flags |= B_ERROR;
				break;
			}
			(*strat)( nbp);
			spl6( );
			until (nbp->b_flags & B_DONE) {
				nbp->b_flags |= B_WANTED;
				sleep( nbp, PRIBIO);
			}
			spl0( );
			if (nbp->b_flags & B_ERROR) {
				if (bp->b_resid == bp->b_bcount) {
					bp->b_error = nbp->b_error;
					bp->b_flags |= B_ERROR;
				}
				break;
			}
			unless (cc -= nbp->b_resid)
				break;
		}
		bp->b_un.b_addr += cc;
		bp->b_resid -= cc;
		bp->b_blkno += btod( cc);
	}

	bp->b_flags |= B_DONE;
	brelse( nbp);
}
