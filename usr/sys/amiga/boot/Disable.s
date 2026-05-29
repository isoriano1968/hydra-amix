#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVODisable,-0x78

	text
	global	Disable

Disable:
	mov.l	%a6,-(%sp)
	mov.l	ABSEXECBASE,%a6
	jsr	_LVODisable(%a6)
	mov.l	(%sp)+,%a6
	rts
