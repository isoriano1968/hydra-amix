## Motorola M68020 interrupt/trap handler


# these offsets depend on layout of struct proc in sys/proc.h and struct classfuncs in sys/class.h
	set	p_clproc,0xe8
	set	p_clfuncs,0xec
	set	cl_trapret,0x4c

	text
	global	p1int
p1int:					# Vector address 64
	movm.l	&0xfffe,-(%sp)		# Save registers
	mov.l	sup_cacr,%d0		# Switch cache mode to supervisor
	mov.l	%d0,%cacr

	mov.w	0xDFF01E,%d0		# Serial line transmit interrupt?
	and.w	&0x0001,%d0		#
	beq	intret			# No
	mov.w	%d0,0xDFF09C		# Dismiss

	jsr	sltint			# Call handler

	bra	intret			# And return


	global	p2int
p2int:					# Vector address 68
	movm.l	&0xfffe,-(%sp)		# Save registers
	mov.l	sup_cacr,%d0		# Switch cache mode to supervisor
	mov.l	%d0,%cacr

	mov.w	0xDFF01E,%d0		# Valid interrupt?
	and.w	&0x0008,%d0		#
	beq	intret			# No
	mov.w	%d0,0xDFF09C		# Dismiss

	mov.l	&0,-(%sp)		# Fake USP to make a pcb structure
	pea	(%sp)			# Pass POINTER to pcb

	lea.l	int2_tbl,%a2
p2loop:
	mov.l	(%a2)+,%d0
	beq	p2end
	mov.l	%d0,%a0
	jsr	(%a0)
	bra	p2loop
p2end:
	add.l	&8,%sp

	bra	intret			# And return


	global	p3int
p3int:					# Vector address 6C
	movm.l	&0xfffe,-(%sp)		# Save registers
	mov.l	sup_cacr,%d0		# Switch cache mode to supervisor
	mov.l	%d0,%cacr

	mov.w	0xDFF01E,%d0		# VerticalBlank interrupt?
	and.w	&0x0020,%d0		#
	beq	intret			# No
	mov.w	%d0,0xDFF09C		# Dismiss

	lea.l	vbinttab,%a2
	sub.l	&4,%sp
vbloop:
	mov.l	(%a2)+,%d0
	beq	vbend
	mov.l	%d0,%a0
	mov.l	(%a2)+,%d0
	mov.l	%d0,(%sp)
	jsr	(%a0)
	bra	vbloop
vbend:
	add.l	&4,%sp

	bra	intret			# And return


	global	p4int
p4int:					# Vector address 70
	movm.l	&0xfffe,-(%sp)		# Save registers
	mov.l	sup_cacr,%d0		# Switch cache mode to supervisor
	mov.l	%d0,%cacr

	mov.w	0xDFF01E,%d0		# Valid interrupt?
	and.w	&0x0780,%d0		#
	beq	intret			# No
	mov.w	%d0,0xDFF09C		# Dismiss

	lsr.w	&7,%d0			# Shift into place
	and.l	&0x0F,%d0		# Isolate bits
	mov.l	%d0,-(%sp)		# Pass argument
	jsr	audiointr		# Call audio handler
	add.l	&4,%sp			# Pop argument

	bra	intret			# And return


#
# Rico has stolen this vector for fast sl
#
	global	p5int
p5int:					# Vector address 74
	movm.l	&0xfffe,-(%sp)		# Save registers

	mov.w	0xDFF01E,%d0		# Serial line receive interrupt?
	and.w	&0x0800,%d0		#
	beq	nop5			# No
	mov.w	%d0,0xDFF09C		# Dismiss

	jsr	slrint

nop5:	movm.l	(%sp)+,&0x7fff
	rte


	global	p6int
p6int:					# Vector address 78
	movm.l	&0xfffe,-(%sp)		# Save registers
	mov.l	sup_cacr,%d0		# Switch cache mode to supervisor
	mov.l	%d0,%cacr

	mov.w	0xDFF01E,%d0		# Valid interrupt?
	and.w	&0x2000,%d0		#
	beq	intret			# No
	mov.w	%d0,0xDFF09C		# Dismiss

	mov.l	&0,-(%sp)		# Fake USP to make a pcb structure
	pea	(%sp)			# Pass POINTER to pcb to aciabintr
	jsr	aciabintr		# Call ACIAB handler
	add.l	&8,%sp

#	bra	intret			# And return


intret:	global	intret
	btst	&5,60(%sp)		# test S bit in SR
	bne 	return			# return if kernel mode intr
	tst.l	runrun			# test cpu reschedule flag
	beq 	ureturn			# check queues before returning
	mov.w	&0x2000,%sr		# enable interrupts
	and.w	&0xf000,66(%sp)		# 0 psuedo vector for qswtch()
	bra.b	utraps

nullvect:	global	nullvect
	movm.l	&0xfffe,-(%sp)		# push D0-D7,A0-A6 on stack
	mov.l	sup_cacr,%d0		# switch cache mode to supervisor
	mov.l	%d0,%cacr
	btst	&5,60(%sp)		# test S bit in SR
	beq	utraps			# process user mode exception

ktraps:	global	ktraps			# else process kernel mode exception

	mov.l	%usp,%a0
	mov.l	%a0,-(%sp)		# push current proc's sp on stack
	jsr	k_trap
	mov.l	(%sp)+,%a0
	mov.l	%a0,%usp		# restore user stack pointer
	tst.l	%d0			# test for stack frame adjustment
	beq	return			# no queue processing on return
					# from kernel mode exception
	bra	stkclear		# rte from 4 word frame

utraps:	global	utraps
	mov.l	%usp,%a0
	mov.l	%a0,-(%sp)	 	# push current proc's sp on stack
	jsr	u_trap
	mov.l	(%sp)+,%a0
	mov.l	%a0,%usp		# restore user stack pointer

ureturn: global	ureturn
				 	# schedule streams queues

	mov.w	%sr,%d0		 	# entering critical section
	mov.w	&0x2700,%sr	 	# mask all interrupts
	tst.b	qrunflag	 	# test for queues enabled
	beq	not_qrun
	tst.b	queueflag	 	# don't run if already in queuerun()
	bne	not_qrun
	addq.b	&1,queueflag	 	# block another call to queuerun()

	mov.w	%d0,%sr		 	# leaving critical section
	jsr	queuerun
	clr.b	queueflag	 	# allow calls to queuerun()
	bra	trap_ret3

not_qrun:	global not_qrun
	mov.w	%d0,%sr		 	# restore status register

trap_ret3:

	mov.l	%usp,%a0
	mov.l	%a0,-(%sp)	 	# push current proc's sp on stack

	mov.l	curproc,%a0
	mov.l	p_clproc(%a0),-(%sp)	# Push first arg to cl_trapret
	mov.l	p_clfuncs(%a0),%a0	# Locate appropriate classfuncs struct
	jsr	([cl_trapret,%a0])	# Class specific cl_trapret call
	add.l	&4,%sp

	jsr	s_trap

	mov.l	(%sp)+,%a0
	mov.l	%a0,%usp		# restore user stack pointer

stkcheck:
					# NOTE**** (sigflag) could be replaced
					# with absolute u-block address
	mov.b	([sigflag]),%d0		# read upper byte of signal save flag (see sys3b.c)
	beq	return

	mov.b	&0,([sigflag])		# clear u.u_sigflag (upper byte only..)

	btst	&7,%d0			# if USTKCLEAR is set,
	bne	stkclear		# rte from 4 word stack frame
	btst	&6,%d0			# if USTKRESTORE is set,
	bne	stkrestore		# restore stack frame from u-block

return:	global	return
	btst	&5,60(%sp)
	bne	return2
	mov.l	cacr,%d0		# setup user mode cacr
	mov.l	%d0,%cacr
return2:
	movm.l	(%sp)+,&0x7fff
	rte

# rte using a 4 word stack frame

stkclear:				# convert stack to 4 word frame
	clr.l	%d0
	mov.w	66(%sp),%d0		# read format&vector
	lsr.w	&12,%d0			# get format (zero high 12 bits of low word)
	beq	return			# no adjustment necessary for 4 word frame

	mov.b	(framesz,%d0.w),%d0	# number of bytes in frame
	add.l	&15*4,%d0		# plus 60 bytes for D0-D7,A0-A6

	mov.l	%sp,%a0
	mov.w	&0x2700,%sr		# mask interrupts while adjusting %sp
	add.l	%d0,%sp			# point below stack frame
	mov.l	64(%a0),-(%sp)		# move pcl and format&vector
	mov.l	60(%a0),-(%sp)		# move psw, and pch
	mov.w	&0,6(%sp)		# zero format
	movm.l	(%a0),&0x7fff		# restore D0-D7,A0-A6
	rte

# reset the 68020 stack frame from the u-block save area before
# returning to user mode

stkrestore:
	mov.l	sigsave,%a2		# address of u.u_sigsave[],see sys3b.c
	mov.w	6(%a2),%d0		# read format&vector of frame
	lsr.w	&12,%d0			# get format (zeroes upper 12 bits of low word

	mov.b	(framesz,%d0.w),%d0	# read frame size

	mov.w	&-8,%d1
	add.w	%d0,%d1			# framesz - 8 bytes
	mov.l	%sp,%a0			# pointer to D0
	sub.w	%d1,%sp			#
	mov.l	%sp,%a1			# new D0 location
	mov.w	&15,%d2			# number of long words  to transfer
	bra	Ldb1
Ldb2:	mov.l	(%a0)+,(%a1)+		# shift D0-D7,A0-A7 by %d1.w bytes
Ldb1:	dbra.w	%d2,Ldb2
	lsr.w	&2,%d0			# number of long words in stack frame
	bra	Ldb3
Ldb4:	mov.l	(%a2)+,(%a1)+		# restore stack frame
Ldb3:	dbra.w	%d0,Ldb4
	bra	return



# void addvbint(void (*func)(long), long arg);
# (adds 'func' to the vbint table, causing 'func' to be called with
#  arg 'arg' at vbint time)

	global	addvbint
addvbint:
	lea.l	vbinttab,%a0
addvbloop:
	mov.l	(%a0),%d0
	beq	addvbfound
	add.l	&8,%a0
	bra	addvbloop
addvbfound:
	movm.l	4(%sp),&3
	movm.l	&3,(%a0)
	rts

# void remvbint(void (*func)(long));
# (removes first occurrence 'func' from the vbint table)

	global	remvbint
remvbint:
	lea.l	vbinttab,%a0
	mov.l	4(%sp),%d0
remvbloop:
	mov.l	(%a0),%d1
	beq	remvbdone
	cmp.l	%d0,%d1
	beq	remvbloop2
	add.l	&8,%a0
	bra	remvbloop
remvbloop2:
	mov.l	8(%a0),(%a0)+
	beq	remvbdone
	mov.l	8(%a0),(%a0)+
	bra	remvbloop2
remvbdone:
	rts


	data
	global	vstksave
vstksave:
	long	0		# storage for saved stack pointer
tc_save:
	long 	0
tc_off:
	long 0x02B02D60 	# E=0,Psize=2K,TI=2/13/6/11
tc_on:	global tc_on
	long 0x82B02D60 	# E=1,Psize=2K,TI=2/13/6/11
tt0_on: global	tt0_on
	long	0x003F0143 # 0x0000000-0x3FFFFFFF FC=Super,R/W,Disable,CI=0
tt1_on: global	tt1_on
	long	0x807F0143 # 0x8000000-0xFFFFFFFF FC=Super,R/W,Disable,CI=0
tt_off:	global	tt_off
	long	0


# struct { void (*func)(long); long arg; } vbinttab[20];
# (The table of vertical blank interrupt servers)

	comm	vbinttab,20*8
