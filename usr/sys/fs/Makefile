#
#	You may delete unused filesystem types from this list
#
#	WARNING:	You must leave either s5/exp or ufs/exp in place
#			to have Amiga UNIX recognize your root filesystem
#			Failure to do so will produce an unuseable kernel.
#

OPT_SUBEXP =	bfs/exp \
		s5/exp \
		xnamfs/exp \
		ufs/exp \
		nfs/exp \
		rfs/exp

exp:    fs.exp $(OPT_SUBEXP)
	ld -r -o exp fs.exp $(OPT_SUBEXP)

clean:
clobber:
