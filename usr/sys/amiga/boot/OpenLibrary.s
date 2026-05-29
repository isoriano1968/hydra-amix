#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVOOpenLibrary,-0x228

	text
	global	OpenLibrary

OpenLibrary:
	mov.l	4(%sp),%a1
	mov.l	8(%sp),%d0
	mov.l	%a6,-(%sp)
	mov.l	ABSEXECBASE,%a6
	jsr	_LVOOpenLibrary(%a6)
	mov.l	(%sp)+,%a6
	mov.l	%d0,%a0
	rts
