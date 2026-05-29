# Copyright (C) 1991, Commodore Business Machines, Inc.
#
# Execute a short assembly language function in the supervisor mode of
# the processor.  Supervisor() does not modify or save registers; the
# user function has full access to the register set.  The user function
# must end with an RTE instruction.
#
# A pointer to the code to execute is passed in a5.
# An additional parameter can be passed in a2.
#

	set	ABSEXECBASE,4
	set	_LVOSupervisor,-0x1e

	text
	global	Supervisor

Supervisor:
	mov.l	%a5,-(%sp)
	mov.l	%a2,-(%sp)
	mov.l	%a6,-(%sp)
	mov.l	16(%sp),%a5
	mov.l	20(%sp),%a2
	mov.l	ABSEXECBASE,%a6
	jsr	_LVOSupervisor(%a6)
	mov.l	(%sp)+,%a6
	mov.l	(%sp)+,%a2
	mov.l	(%sp)+,%a5
	mov.l	%d0,%a0
	rts
