#  Copyright (C) 1991, Commodore Business Machines, Inc.

	text
	global	memcpy

memcpy:
	mov.l	4(%sp),%a1
	mov.l	8(%sp),%a0
	mov.l	12(%sp),%d0
	bra.b	memcpystart
memcpyloop:
	mov.b	(%a0)+,(%a1)+
memcpystart:
	sub.l	&1,%d0
	bcc.b	memcpyloop
	rts

