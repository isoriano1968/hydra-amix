

////////
//
// A2232 serial card
//	Resident 65C02 code for use by Amix.
//


include( startup.m4)
include( dev.m4)


// r0condition
//
define(	BREAK_DETECTED,	1)
define(	CARR_DETECTED,	2)
define(	CARR_DROPPED,	3)
define( RECEIVER_SYNC,	4)

define( XOFF,		0x13)


define( scanline, `
	lda	acia$1+a_status
	and	#1<<3
	beq	2f
	lda	acia$1+a_status
	and	#1<<1
	beq	1f
	lda	acia$1+a_data
	bne	2f
	lda	#1
	sta	_r$1break
	ldx	_r$1head
	stx	_r$1breakhead
	jmp	2f
1:	ldx	_r$1head
	lda	acia$1+a_data
	sta	r$1buf, x
	inx
	stx	_r$1head
	and	#0x7F
	cmp	#XOFF
	bne	2f
	lda	_c$1xon
	sta	t$1disable

2:	lda	t$1tandem
	beq	1f
	lda	acia$1+a_status
	and	#1<<4
	beq	3f
	lda	cia+c_cts
	and	#1<<$1
	bne	3f
	lda	t$1tandem
	stz	t$1tandem
	jmp	2f
1:	ldx	t$1tail
	cpx	t$1head
	beq	3f
	lda	t$1disable
	bne	3f
	lda	acia$1+a_status
	and	#1<<4
	beq	3f
	lda	cia+c_cts
	and	#1<<$1
	bne	3f
	lda	t$1buf, x
	inx
	stx	t$1tail
2:	sta	acia$1+a_data

3:'
)

define( housekeep, `
	lda	cia+c_cd
	and	#1<<$1
	eor	_cd$1
	beq	1f
	sta	_cd$1changed
	eor	_cd$1
	sta	_cd$1
	lda	_r$1head
	sta	_cd$1head

1:	lda	r$1condition
	bne	1f
	lda	_r$1break
	beq	2f
	lda	_r$1breakhead
	sta	_cd$1head
	sta	r$1head
	lda	#BREAK_DETECTED
	sta	r$1condition
	stz	_r$1break
	jmp	9f
2:	lda	_cd$1changed
	beq	4f
	lda	_cd$1head
	sta	r$1head
	ldx	#CARR_DETECTED
	lda	_cd$1
	beq	3f
	ldx	#CARR_DROPPED
3:	stx	r$1condition
	stz	_cd$1changed
	jmp	9f
4:	lda	r$1sync
	beq	2f
	stz	r$1sync
	lda	#RECEIVER_SYNC
	sta	r$1condition
	jmp	9f

2:	lda	_r$1head
	sta	r$1head

1:	lda	c$1setparams
	beq	1f
	stz	c$1setparams
	lda	c$1control
	sta	acia$1+a_control
	lda	c$1command
	sta	acia$1+a_command
	lda	c$1xon
	sta	_c$1xon
	stz	t$1disable
	jmp	9f

1:	lda	t$1flush
	beq	9f
	lda	t$1head
	sta	t$1tail
	stz	t$1disable
	stz	t$1flush

9:'
)

define( public, `
	r$1head:	.byte	0
	r$1condition:	.byte	0
	r$1sync:	.byte	0
	t$1disable:	.byte	0
	t$1head:	.byte	0
	t$1tail:	.byte	0
	t$1tandem:	.byte	0
	t$1flush:	.byte	0
	c$1setparams:	.byte	0
	c$1control:	.byte	0
	c$1command:	.byte	0
	c$1xon:		.byte	0'
)

define( private, `
	_r$1head:	.byte	0
	_cd$1:		.byte	0
	_cd$1changed:	.byte	1
	_cd$1head:	.byte	0
	_r$1break:	.byte	0
	_r$1breakhead:	.byte	0
	_c$1xon:	.byte	0'
)

define( `for_all_lines', `
	define( `__fal', `$1')
	__fal( 0)
	__fal( 1)
	__fal( 2)
	__fal( 3)
	__fal( 4)
	__fal( 5)
	__fal( 6)'
)


	.seg 0x3FFC	// same as 0xFFFC
reset_vector:
	.word	main


	.seg 0x3000
main:
	for_all_lines( `
		jsr	scanlines
		housekeep( $1)'
	)
	jmp	main

scanlines:
	for_all_lines( `
		scanline( $1)')
	rts


	.seg 0x0000
	for_all_lines( `
		public( $1)')
	for_all_lines( `
		private( $1)')


	.seg 0x0100
	.block	0x0100


	.seg 0x0200
	for_all_lines( `
		t$1buf:	.block	0x0100')
	for_all_lines( `
		r$1buf:	.block	0x0100')
