# Halt or reboot the machine
#


# A do-nothing MMU root pointer (includes the following long as well)
nullrp:	long	0x7fff0001
zero:	long	0


haltmsg:
	byte	'T,'h,'e,' ,'s,'y,'s,'t,'e,'m,' ,'i,'s,' 
	byte	'h,'a,'l,'t,'e,'d,';,' ,'y,'o,'u,' ,'m,'a,'y,' 
	byte	'r,'e,'b,'o,'o,'t,' ,'o,'r,' ,'t,'u,'r,'n,' 
	byte	'o,'f,'f,' ,'p,'o,'w,'e,'r,'.,0

	global	rtnfirm			# rtnfirm(void)  ==  haltsys(1)
rtnfirm:
	mov.l	&1,4(%sp)		# yucky


# void haltsys(int thenwhat)
#   thenwhat == 0	halt
#		1	reboot
	global	haltsys
haltsys:
	mov.l	4(%sp),%d2
	bne	nomsg
	pea	haltmsg
	jsr	printf
nomsg:

#   NOTE
#	Rebooting an Amiga in software is very tricky.	Differing memory
#	configurations and processor cards require careful treatment.  Plus,
#	the Exec 1.4 roms do Stupid MMU Tricks.  And, so that this same code
#	will work on pre-AmigaDos1.4 systems, the A2620 ROM is used if it was
#	used to boot the kernel (determined by "boot_arg0").
#
#	Actually, come to think of it, rebooting an Amiga is not possible.
#	This function just trashes memory, executes some randomly selected
#	code, and puts lots of garbage on the screen as a hint to the user to
#	reset the machine.

	mov.w	&0x2700,%sr		# Turn off interrupts

	lea	zero,%a0
	pmove	(%a0),%tc		# Turn off MMU

	lea	nullrp,%a0
	pmove	(%a0),%crp		# Turn off MMU some more
	pmove	(%a0),%srp		# Really, really, turn off MMU

	mov.l	&0,%d0			# Turn off caches
	mov.l	%d0,%cacr

	# AmigaDOS Bletcherosity on A3000
	bset	&7,0xde0002


	tst.l	%d2			# Test halt/boot flag
	bne	reboot			# Zero=halt, NZ=boot

halt:
	bclr	&6,0xbfee01		# If we are being rebooted by the
					# keyboard, ack now that it's safe.
	bra	halt

reboot:
	cmp.l	boot_arg0,&1		# Did we boot using 2620 ROM?
	bne	new_funky_reboot	# 
	jmp	([0xF80028])		# 2620 ROM Reboot vector

	longeven
new_funky_reboot:
	mov.w	&2,%a0
	nop
	nop
	reset
	jmp	(%a0)

#	set	MAGIC_ROMEND,0x01000000	# End of Kickstart ROM
#	set	MAGIC_SIZEOFFSET,-0x14	# Offset from end of ROM to Kickstart size
#
## -------------- MagicResetCode ---------DO NOT CHANGE-----------------------
#	longeven			# IMPORTANT! Longword align!
#	lea.l	MAGIC_ROMEND,%a0	# (end of ROM)
#	sub.l	MAGIC_SIZEOFFSET(%a0),%a0 # (end of ROM)-(ROM size)=PC
#	mov.l	4(%a0),%a0		# Get Initial Program Counter
#	sub.l	&2,%a0			# RESET instructions is here
#	reset				# first RESET instruction
#	jmp	(%a0)			# CPU Prefetch executes this
## NOTE: the RESET and JMP instructions must share a longword!
