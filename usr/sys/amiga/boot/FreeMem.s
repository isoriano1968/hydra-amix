#  Copyright (C) 1991, Commodore Business Machines, Inc.

	set	ABSEXECBASE,4
	set	_LVOFreeMem,-0xd2

	text
	global	FreeMem

FreeMem:
	mov.l	4(%sp),%a1
	mov.l	8(%sp),%d0
	mov.l	%a6,-(%sp)
	mov.l	ABSEXECBASE,%a6
	jsr	_LVOFreeMem(%a6)
	mov.l	(%sp)+,%a6
	rts

