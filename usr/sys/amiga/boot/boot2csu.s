#  Copyright (C) 1991, Commodore Business Machines, Inc.
#
#  This is the bootstrap startup module which gets control from the
#  system after the bootstrap has been loaded into memory.
#
#  After the bsr to main the stack is:
#
#		|			|
#		|-----------------------|
#		| struct IOStdReq *	|
#		|-----------------------|
#	sp ->	| _start return address	|
#		|-----------------------|
#		|			|
#		|-----------------------|
#		|			|



	text

	global	_start

_start:
	mov.l	4(%sp),-(%sp)
	bsr	main
	add.l	&4,%sp
	tst.l	%d0
	bne	success
	mov.l	&0x87654321,%d0
	rts

success:
	mov.l	%d0,%a0
	mov.l	&0,%d0
	rts
