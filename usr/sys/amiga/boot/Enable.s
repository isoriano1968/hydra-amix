#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVOEnable,-0x7e

	text
	global	Enable

Enable:
	mov.l	%a6,-(%sp)
	mov.l	ABSEXECBASE,%a6
	jsr	_LVOEnable(%a6)
	mov.l	(%sp)+,%a6
	rts
