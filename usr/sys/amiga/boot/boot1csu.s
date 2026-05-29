#  Copyright (C) 1991, Commodore Business Machines, Inc.
#
#  First level bootstrap C startup module.  This is the code which gets
#  control from the system after the bootstrap has been loaded into memory.
#  On entry, a1 contains a pointer to an I/O request structure, which is
#  copied into the proper stack location to appear as the first parameter
#  to the C main() routine.
#
#  At entry, the stack and a1 look like:
#
#		|			|
#		|-----------------------|
#	sp ->	| sys return address	|	a1 = (struct IOStdReq *)
#		|-----------------------|
#		|			|
#
#  After the bsr to main the stack is:
#
#		|			|
#		|-----------------------|
#		| sys return address	|
#		|-----------------------|
#		| struct IOStdReq *	|
#		|-----------------------|
#	sp ->	| _start return address	|
#		|-----------------------|
#		|			|
#		|-----------------------|
#		|			|
#
#  If main() returns zero, then something went wrong and the second
#  level bootstrap was not called, or it returned zero.  Otherwise,
#  a nonzero number is returned to indicate success.

	text
	global	_start

_start:
	pea	(%sp)			# Set up main() second parameter.
	mov.l	%a1,-(%sp)		# Set up main() first parameter.
	bsr	main			# Call C code.
	add.w	&8,%sp			# Pop stack.
	tst.l	%d0			# Check return from main()
	bne	success			# If not zero then all is ok.
	mov.l	&0x87654321,%d0		# Bad news
	rts

success:
	mov.l	%d0,%a0			# Return copied to ptr reg.
	mov.l	&0,%d0			# Success.
	rts
