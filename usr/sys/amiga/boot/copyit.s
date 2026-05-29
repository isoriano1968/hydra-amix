# Copyright (C) 1991, Commodore Business Machines, Inc.
# Patchlevel: copyit.s_1.1
#
# This routine is the last non-UNIX piece of code executed during
# the boot process.  It is used to copy the UNIX ".text", ".rodata",
# and ".data" sections from where they are currently located in
# AmigaDOS allocated memory, to their final locations.  Once the
# copy starts, AmigaDOS can be considered to be dead, since we
# simply overwrite whatever may have happened to be in the destination
# regions.
#
# Several assumptions are made by this code, which are true for
# the current development tools:
#
# (1)	The kernel consists of one or more sections, ".text", ".rodata",
#	and ".data", which are loaded sequentially in memory, in that
#	order.
#
# (2)	All sections must be copied the same direction in memory to
#	their final location.  I.E. if the text section is copyied
#	from higher addresses to lower addresses, then the other two
#	are copied the same direction.
#		
# Copyit must be called in supervisor mode with a2 pointing to a
# copyinfo struct (this is handled by Supervisor.s).  Note that the
# copyit() function code is memcpy()'d into chip memory, and it is
# that copy that actually gets executed.  We also have to set up
# a new stack in chip memory, since the old one may get overwritten
# during the copy of one of the UNIX sections.

# The copyinfo structure is also set up in chip memory, and contains
# the information described by the following C structure:
#
# struct copyinfo {
#    unsigned long ci_sp;	/* Space in chip mem for stack */
#    unsigned long ci_entry;	/* Entry point in kernel */
#    unsigned long ci_d0;	/* Value to place in d0 for kernel */
#    unsigned long ci_d1;	/* Value to place in d1 for kernel */
#    unsigned long ci_tbuf;	/* Source address of .text */
#    unsigned long ci_tvaddr;	/* Destination address of .text */
#    unsigned long ci_tsize;	/* Size of .text */
#    unsigned long ci_rbuf;	/* Source address of .rodata */
#    unsigned long ci_rvaddr;	/* Destination address of .rodata */
#    unsigned long ci_rsize;	/* Size of .rodata */
#    unsigned long ci_dbuf;	/* Source address of .data */
#    unsigned long ci_dvaddr;	/* Destination address of .data */
#    unsigned long ci_dsize;	/* Size of .data */
# };

# The below offsets must match the structure in boot2.c, included
# above for reference.

	.set	ci_sp,		0
	.set	ci_entry,	4
	.set	ci_d0,		8
	.set	ci_d1,		12
	.set	ci_tbuf,	16
	.set	ci_tvaddr,	20
	.set	ci_tsize,	24
	.set	ci_rbuf,	28
	.set	ci_rvaddr,	32
	.set	ci_rsize,	36
	.set	ci_dbuf,	40
	.set	ci_dvaddr,	44
	.set	ci_dsize,	48

	.set	ABSEXECBASE,	4

	.text
	.global	copyit
	.global	copyitend

copyit:
	mov.w	&0x2700,%sr

	lea	zero(%pc),%a0
	pmove	(%a0),%tc		# Turn off MMU
	lea	nullrp(%pc),%a0
	pmove	(%a0),%crp		# Turn off MMU some more
	pmove	(%a0),%srp		# Really, really, turn off MMU

# Turn off 68030 TT registers

	btst	&2,(0x129,[ABSEXECBASE]) # AFB_68030,SysBase->AttnFlags
	beq	nott			# Skip TT registers if not 68030.
	lea	zero(%pc),%a0
	pmove	(%a0),%tt0
	pmove	(%a0),%tt1

nott:
	mov.l	(ci_sp,%a2),%sp		# Switch to stack in chip mem

# Check to see if the ".text" section is going to be copied up in memory
# or down in memory (see assumptions listed at top of this file).
# Note that when copying up in memory we have to start from the tail
# of the ".data" section, copying all the sections in reverse order and
# from the tail of each section.  When copying down in memory, we start
# at the head of the ".text" section, copying all the sections in forward
# order and from the head of each section.

	mov.l	(ci_tbuf,%a2),%a0	# a0 = Pointer to .text buffer.
	mov.l	(ci_tvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	cmp.l	%a0,%a1			# See which direction to shift kernel
	bcc.b	copydown		#  in memory, up or down. (%a0-%a1)

copyup:

	mov.l	(ci_dbuf,%a2),%a0	# a0 = Pointer to .data buffer.
	mov.l	(ci_dvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	mov.l	(ci_dsize,%a2),%d0	# d0 = Size of .data
	bsr	docopyup

	mov.l	(ci_rbuf,%a2),%a0	# a0 = Pointer to .rodata buffer.
	mov.l	(ci_rvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	mov.l	(ci_rsize,%a2),%d0	# d0 = Size of .rodata
	bsr	docopyup

	mov.l	(ci_tbuf,%a2),%a0	# a0 = Pointer to .text buffer.
	mov.l	(ci_tvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	mov.l	(ci_tsize,%a2),%d0	# d0 = Size of .text
	bsr	docopyup

	bra.b	callkernel

copydown:

	mov.l	(ci_tbuf,%a2),%a0	# a0 = Pointer to .text buffer.
	mov.l	(ci_tvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	mov.l	(ci_tsize,%a2),%d0	# d0 = Size of .text
	bsr	docopydown

	mov.l	(ci_rbuf,%a2),%a0	# a0 = Pointer to .rodata buffer.
	mov.l	(ci_rvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	mov.l	(ci_rsize,%a2),%d0	# d0 = Size of .rodata
	bsr	docopydown

	mov.l	(ci_dbuf,%a2),%a0	# a0 = Pointer to .data buffer.
	mov.l	(ci_dvaddr,%a2),%a1	# a1 = Virtual addr to move buffer to.
	mov.l	(ci_dsize,%a2),%d0	# d0 = Size of .data
	bsr	docopydown

# All of the kernel sections have been moved to their final destinations
# now.  All that is left to do is set up the arguments to the kernel
# and jump to it's entry point.

callkernel:

	mov.l	(ci_entry,%a2),%a0	# Get kernel entry point
	mov.l	(ci_d0,%a2),%d0		#  and first argument to kernel
	mov.l	(ci_d1,%a2),%d1		#  and second argument to kernel
	jmp	(%a0)			#  and go for it.

# Copy specified number of bytes of data from source address to 
# destination address, copying down in memory (I.E. from start
# of the region to copy).
#
# Arguments are:
#
#	a0	Source address
#	a1	Destination address
#	d0	Number of bytes (may be zero)
#

docopydown:

	tst.l	%d0			# Check to see if we have any work
	beq.b	docopydowndone		#  and just return if not.

docopydownmore:				# Shuffle kernel down in memory

	mov.b	(%a0)+,(%a1)+		#  moving bytes from buffer to dest
	sub.l	&1,%d0			#  one byte at a time from head
	bcc.b	docopydownmore		#  until all bytes are moved.

docopydowndone:

	rts


# Copy specified number of bytes of data from source address to 
# destination address, copying up in memory (I.E. from tail
# of the region to copy).
#
# Arguments are:
#
#	a0	Source address
#	a1	Destination address
#	d0	Number of bytes (may be zero)
#

docopyup:

	tst.l	%d0			# Check to see if we have any work
	beq.b	docopyupdone		#  and just return if not.
	add.l	%d0,%a0			# Copy up from tail of buffer
	add.l	%d0,%a1			#  to tail of kernel memory.

docopyupmore:				# Shuffle kernel up in memory

	mov.b	-(%a0),-(%a1)		#  moving bytes from buffer to dest
	sub.l	&1,%d0			#  one byte at a time from tail
	bcc.b	docopyupmore		#  until all bytes are moved.

docopyupdone:

	rts


# A do-nothing MMU root pointer (includes the following long as well)

nullrp:	long	0x7fff0001
zero:	long	0

# This label is used to mark the end of the copy code, so that the
# size of the code can be computed.

copyitend:
