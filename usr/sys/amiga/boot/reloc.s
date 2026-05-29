#  Copyright (C) 1991, Commodore Business Machines, Inc.

# reloc: for C code to "relocate" non-position-independent addresses.

	text
	global	reloc

reloc:
	lea.l	reloc(%pc),%a0
	sub.l	&reloc,%a0
	add.l	4(%sp),%a0
	mov.l	%a0,%d0
	rts
