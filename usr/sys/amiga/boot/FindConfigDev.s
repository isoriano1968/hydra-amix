#  Copyright (C) 1991, Commodore Business Machines, Inc.
#  Written by Lawrence E. Rosenman
#  Patchlevel: FindConfigDev.s_1.1
	set	ABSEXECBASE,4
	set	LIB_REVISION,33
	set	_LVOFindConfigDev,-0x48
	set	_LVOOpenLibrary,-0x228
	set	_LVOCloseLibrary,-0x19E

	text
	global	FindConfigDev

FindConfigDev:

	mov.l	%d7,-(%sp)		# Preserve old d7
	mov.l	%a5,-(%sp)		# Preserve old a5
	mov.l	%a6,-(%sp)		# Preserve old frame pointer

	mov.l	ABSEXECBASE,%a6		# Set up exec.library base
	lea	(ELib.w,%pc),%a1	# Expansion library name string
	mov.l	&LIB_REVISION,%d0	# Expansion library version
	jsr	_LVOOpenLibrary(%a6)	# Open expansion library
	mov.l	%d0,%a6			# Set up expansion.library base

	mov.l	16(%sp),%a0		# Get oldconfigdev 
	mov.l	20(%sp),%d0		# manuf. number
	mov.l	24(%sp),%d1		# prod number 
	jsr	_LVOFindConfigDev(%a6)	# get configdev 
	mov.l	%d0,%d2			# Save the return in d2

	mov.l	%a6,%a1			# Base of library to close
	mov.l	ABSEXECBASE,%a6		# Set up exec.library base
	jsr	_LVOCloseLibrary(%a6)	# Don't need expansion anymore
	mov.l	(%sp)+,%a6		# Restore old frame pointer
	mov.l	(%sp)+,%a5		# Restore old a5
	mov.l	(%sp)+,%d7		# Restore old d7
	mov.l	%d2,%d0			# Get FindConfigDev return value
	mov.l	%d0,%a0			# Copy rtn value to pointer rtn
	rts				# Back to caller

ELib:
	.string "expansion.library"
