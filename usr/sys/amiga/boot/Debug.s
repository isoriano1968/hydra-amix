#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVODebug,-0x72

	text
	global	Debug

Debug:
	mov.l	4(%sp),%d0	# is d0 really correct?
	mov.l	%a6,-(%sp)
	mov.l	ABSEXECBASE,%a6
	jsr	_LVODebug(%a6)
	mov.l	(%sp)+,%a6
	rts
