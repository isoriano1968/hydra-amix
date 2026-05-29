#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	LIB_REVISION,33
	set	_LVODisplayAlert,-0x5A
	set	_LVOOpenLibrary,-0x228
	set	_LVOCloseLibrary,-0x19E

	text
	global	DisplayAlert

DisplayAlert:

	mov.l	%d7,-(%sp)		# Preserve old d7
	mov.l	%a5,-(%sp)		# Preserve old a5
	mov.l	%a6,-(%sp)		# Preserve old frame pointer

	mov.l	ABSEXECBASE,%a6		# Set up exec.library base
	lea	(ILib.w,%pc),%a1	# Intuition library name string
	mov.l	&LIB_REVISION,%d0	# Intuition library version
	jsr	_LVOOpenLibrary(%a6)	# Open intuition library
	mov.l	%d0,%a6			# Set up intuition.library base

	mov.l	16(%sp),%d0		# Get Alert number
	mov.l	20(%sp),%a0		# Pointer to Alert message strings
	mov.l	24(%sp),%d1		# Height of Alert box
	jsr	_LVODisplayAlert(%a6)	# Display the alert
	mov.l	%d0,%d2			# Save the return in d2

	mov.l	%a6,%a1			# Base of library to close
	mov.l	ABSEXECBASE,%a6		# Set up exec.library base
	jsr	_LVOCloseLibrary(%a6)	# Don't need intuition anymore
	mov.l	(%sp)+,%a6		# Restore old frame pointer
	mov.l	(%sp)+,%a5		# Restore old a5
	mov.l	(%sp)+,%d7		# Restore old d7
	mov.l	%d2,%d0			# Get DisplayAlert return value
	mov.l	%d0,%a0			# Copy rtn value to pointer rtn
	rts				# Back to caller

ILib:
	.string "intuition.library"
