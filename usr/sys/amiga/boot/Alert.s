#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVOAlert,-0x6c

	text
	global	Alert

Alert:
	mov.l	%d7,-(%sp)		# Preserve old d7
	mov.l	%a5,-(%sp)		# Preserve old a5
	mov.l	%a6,-(%sp)		# Preserve old frame pointer
	mov.l	16(%sp),%d7		# Get alert number
	mov.l	20(%sp),%a5		# Get parameters
	mov.l	ABSEXECBASE,%a6		# Set up for call to exec
	jsr	_LVOAlert(%a6)		# Call the alert function
	mov.l	(%sp)+,%a6		# Restore old frame pointer
	mov.l	(%sp)+,%a5		# Restore old a5
	mov.l	(%sp)+,%d7		# Restore old d7
	mov.l	%d0,%a0			# Copy rtn value to pointer rtn
	rts				# Back to caller

