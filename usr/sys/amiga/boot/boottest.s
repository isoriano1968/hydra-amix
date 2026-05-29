# Copyright (C) 1991, Commodore Business Machines, Inc.
#
# The standard AmigaDOS 1.3 boot code.  Can be used to replicate the
# AmigaDOS bootblocks byte-for-byte.


	set	_LVOFindResident,-96
	set	LIB_REVISION,22

_start:
	lea	(DOSLib.w,%pc),%a1		# 0x43fa0018
	jsr	(_LVOFindResident.w,%a6)	# 0x4eaeffa0
	tst.l	%d0				# 0x4a80
	beq.b	NoDOS				# 0x670a
	mov.l	%d0,%a0				# 0x2040
	mov.l	(LIB_REVISION.w,%a0),%a0	# 0x20680016
	mov.l	&0,%d0				# 0x7000
Done:
	rts					# 0x4e75
NoDOS:
	mov.l	&-1,%d0				# 0x70ff
	bra.b	Done				# 0x60fa
DOSLib:
	.string	"dos.library"			# 0x646f732e, 0x6c696272,
						# 0x61727900
	.short	0x0000				# 0x0000  (padding)
