#  Copyright (C) 1991, Commodore Business Machines, Inc.
#
#  Complete an I/O request.
#  A nonzero return indicates an error.

	set	ABSEXECBASE,4
	set	_LVODisable,-0x78
	set	_LVODoIO,-0x1c8

	text
	global	DoIO

DoIO:
	mov.l	4(%sp),%a1
	mov.l	%a6,-(%sp)
	mov.l	ABSEXECBASE,%a6
	jsr	_LVODoIO(%a6)
	mov.l	(%sp)+,%a6
	mov.l	%d0,%a0
	rts

