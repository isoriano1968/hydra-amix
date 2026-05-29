#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVOAllocMem,-0xc6

	text
	global	AllocMem

AllocMem:
	mov.l	4(%sp),%d0		# Get allocation byte size
	mov.l	8(%sp),%d1		# Get allocation attributes
	mov.l	%a6,-(%sp)		# Push old library base on stack
	mov.l	ABSEXECBASE,%a6		# Set up exec.library base
	jsr	_LVOAllocMem(%a6)	# Call AllocMem
	mov.l	(%sp)+,%a6		# Pop old library base from stack
	mov.l	%d0,%a0			# Copy result to ptr rtn reg
	rts				# Return to caller
