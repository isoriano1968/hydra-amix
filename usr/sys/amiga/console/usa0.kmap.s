define(KA_CHR,	0x00000000)		# Return a character
define(KA_STR8,	0x10000000)		# Return a string of 8-bit chars
define(KA_STR16,0x20000000)		# Return a string of 16-bit chars
define(KA_STR32,0x30000000)		# Return a string of 32-bit chars
define(KA_STAT,	0x50000000)		# Set a state bit
define(KA_TAB,	0x60000000)		# Lookup table
define(KA_NOP,	0x70000000)		# Do nothing
define(KF_NOREP,0x01000000)		# Do not repeat
define(KM_MAGIC,0x2a4b)			# Magic number

define(O,off_$1)
define(OLAB,$1:	set	off_$1`,'$1 - builtinkmap)
define(C,KA_CHR+$1)
define(N,KA_CHR+KF_NOREP+$1)
define(NOP,KA_NOP)
define(S,KA_STAT+$1)
define(S8,KA_STR8+O(str$1))
define(T,KA_TAB+O(tab$1))

define(M0D,0xfe0001)
define(M1D,0xfe0101)
define(M2D,0xfe0201)
define(M0U,0xfe0000)
define(M1U,0xfe0100)
define(M2U,0xfe0200)

	text
builtinkmap:	global	builtinkmap

	short	KM_MAGIC
	byte	0
	byte	0
	long	O(KEND)

	byte	0x60, 0xff, 0xff, 0xff		# left shift
	byte	0x61, 0xff, 0xff, 0xff		# right shift
	byte	0x62, 0xff, 0xff, 0xff		# caps lock
	byte	0x63, 0xff, 0xff, 0xff		# control
	byte	0x64, 0xff, 0xff, 0xff		# left alt
	byte	0x65, 0xff, 0xff, 0xff		# right alt
	byte	0x66, 0xff, 0xff, 0xff		# left amiga
	byte	0x67, 0xff, 0xff, 0xff		# right amiga
	byte	0x60, 0x61, 0xff, 0xff		# shift
	byte	0x64, 0x65, 0xff, 0xff		# alt
	byte	0x66, 0x67, 0xff, 0xff		# amiga
	byte	0x60, 0x61, 0x62, 0xff		# caps or shift
	byte	0x64, 0x65, 0x66, 0xff		# "meta" (alt or left-amiga)
	byte	0xff, 0xff, 0xff, 0xff
	byte	0xff, 0xff, 0xff, 0xff
	byte	0xff, 0xff, 0xff, 0xff

long	T(bq),	T(1),	T(2),	T(3),	T(4),	T(5),	T(6),	T(7)	#00-07
long	T(8),	T(9),	T(0),	T(hyph),T(eq),	T(bsl),	NOP,	C('0)	#08-0f
long	T(q),	T(w),	T(e),	T(r),	T(t),	T(y),	T(u),	T(i)	#10-17
long	T(o),	T(p),	T(lbr),	T(rbr),	NOP,	C('1),	C('2),	C('3)	#18-1f
long	T(a),	T(s),	T(d),	T(f),	T(g),	T(h),	T(j),	T(k)	#20-27
long	T(l),	T(sem),	T(quot),NOP,	NOP,	C('4),	C('5),	C('6)	#28-2f
long	NOP,	T(z),	T(x),	T(c),	T(v),	T(b),	T(n),	T(m)	#30-37
long	T(com),	T(per),	T(sl),	NOP,	C('.),	C('7),	C('8),	C('9)	#38-3f
long	T(spc),	T(bsp),	T(tab),	T(ent),	T(ret),	T(esc),	T(del),	NOP	#40-47
long	NOP,	NOP,	C('-),	NOP,	T(ua),	T(da),	T(ra),	T(la)	#48-4f
long	T(f1),	T(f2),	T(f3),	T(f4),	T(f5),	T(f6),	T(f7),	T(f8)	#50-57
long	T(f9),	T(f10),	C(0x28),C(0x29),C('/),	C('*),	C('+),	T(hlp)	#58-5f
long	NOP,	NOP,	NOP,	NOP,	T(lalt),T(ralt),NOP,	NOP	#60-67
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#68-6f
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#70-77
long	NOP,	NOP,	NOP,	NOP,	N(M0D),	N(M1D),	N(M2D),	NOP	#78-7f
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#80-87
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#88-8f
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#90-97
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#98-9f
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#a0-a7
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#a8-af
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#b0-b7
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#b8-bf
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#c0-c7
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#c8-cf
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#d0-d7
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#d8-df
long	NOP,	NOP,	NOP,	NOP,	T(lalu),T(ralu),NOP,	NOP	#e0-e7
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#e8-ef
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#f0-f7
long	NOP,	NOP,	NOP,	NOP,	N(M0U),	N(M1U),	N(M2U),	NOP	#f8-ff

OLAB(st_shift)
	long	0x0100, 0x0000

OLAB(st_meta)
	long	0x1000, 0x0000

OLAB(st_shift_alt)
	long	0x0200, 0x0100, 0x0000

OLAB(st_shift_meta)
	long	0x1100, 0x0100, 0x1000, 0x0000

OLAB(st_ctl_meta)
	long	0x1008, 0x0008, 0x1000, 0x0000

OLAB(st_shift_ctl_meta)
	long	0x1008, 0x0008, 0x1100, 0x0100, 0x1000, 0x0000

OLAB(st_caps_ctl_meta)
	long	0x1008, 0x0008, 0x1800, 0x0800, 0x1000, 0x0000

OLAB(taba)
	long	O(st_caps_ctl_meta)
	long	C(0x81), C(0x01)			# meta/ctl, ctl
	long	C(0xc1), C(0x41)			# meta/caps, caps
	long	C(0xe1), C(0x61)			# meta, plain

OLAB(tabb)
	long	O(st_caps_ctl_meta)
	long	C(0x82), C(0x02)			# meta/ctl, ctl
	long	C(0xc2), C(0x42)			# meta/caps, caps
	long	C(0xe2), C(0x62)			# meta, plain

OLAB(tabc)
	long	O(st_caps_ctl_meta)
	long	C(0x83), C(0x03)			# meta/ctl, ctl
	long	C(0xc3), C(0x43)			# meta/caps, caps
	long	C(0xe3), C(0x63)			# meta, plain

OLAB(tabd)
	long	O(st_caps_ctl_meta)
	long	C(0x84), C(0x04)			# meta/ctl, ctl
	long	C(0xc4), C(0x44)			# meta/caps, caps
	long	C(0xe4), C(0x64)			# meta, plain

OLAB(tabe)
	long	O(st_caps_ctl_meta)
	long	C(0x85), C(0x05)			# meta/ctl, ctl
	long	C(0xc5), C(0x45)			# meta/caps, caps
	long	C(0xe5), C(0x65)			# meta, plain

OLAB(tabf)
	long	O(st_caps_ctl_meta)
	long	C(0x86), C(0x06)			# meta/ctl, ctl
	long	C(0xc6), C(0x46)			# meta/caps, caps
	long	C(0xe6), C(0x66)			# meta, plain

OLAB(tabg)
	long	O(st_caps_ctl_meta)
	long	C(0x87), C(0x07)			# meta/ctl, ctl
	long	C(0xc7), C(0x47)			# meta/caps, caps
	long	C(0xe7), C(0x67)			# meta, plain

OLAB(tabh)
	long	O(st_caps_ctl_meta)
	long	C(0x88), C(0x08)			# meta/ctl, ctl
	long	C(0xc8), C(0x48)			# meta/caps, caps
	long	C(0xe8), C(0x68)			# meta, plain

OLAB(tabi)
	long	O(st_caps_ctl_meta)
	long	C(0x89), C(0x09)			# meta/ctl, ctl
	long	C(0xc9), C(0x49)			# meta/caps, caps
	long	C(0xe9), C(0x69)			# meta, plain

OLAB(tabj)
	long	O(st_caps_ctl_meta)
	long	C(0x8a), C(0x0a)			# meta/ctl, ctl
	long	C(0xca), C(0x4a)			# meta/caps, caps
	long	C(0xea), C(0x6a)			# meta, plain

OLAB(tabk)
	long	O(st_caps_ctl_meta)
	long	C(0x8b), C(0x0b)			# meta/ctl, ctl
	long	C(0xcb), C(0x4b)			# meta/caps, caps
	long	C(0xeb), C(0x6b)			# meta, plain

OLAB(tabl)
	long	O(st_caps_ctl_meta)
	long	C(0x8c), C(0x0c)			# meta/ctl, ctl
	long	C(0xcc), C(0x4c)			# meta/caps, caps
	long	C(0xec), C(0x6c)			# meta, plain

OLAB(tabm)
	long	O(st_caps_ctl_meta)
	long	C(0x8d), C(0x0d)			# meta/ctl, ctl
	long	C(0xcd), C(0x4d)			# meta/caps, caps
	long	C(0xed), C(0x6d)			# meta, plain

OLAB(tabn)
	long	O(st_caps_ctl_meta)
	long	C(0x8e), C(0x0e)			# meta/ctl, ctl
	long	C(0xce), C(0x4e)			# meta/caps, caps
	long	C(0xee), C(0x6e)			# meta, plain

OLAB(tabo)
	long	O(st_caps_ctl_meta)
	long	C(0x8f), C(0x0f)			# meta/ctl, ctl
	long	C(0xcf), C(0x4f)			# meta/caps, caps
	long	C(0xef), C(0x6f)			# meta, plain

OLAB(tabp)
	long	O(st_caps_ctl_meta)
	long	C(0x90), C(0x10)			# meta/ctl, ctl
	long	C(0xd0), C(0x50)			# meta/caps, caps
	long	C(0xf0), C(0x70)			# meta, plain

OLAB(tabq)
	long	O(st_caps_ctl_meta)
	long	C(0x91), C(0x11)			# meta/ctl, ctl
	long	C(0xd1), C(0x51)			# meta/caps, caps
	long	C(0xf1), C(0x71)			# meta, plain

OLAB(tabr)
	long	O(st_caps_ctl_meta)
	long	C(0x92), C(0x12)			# meta/ctl, ctl
	long	C(0xd2), C(0x52)			# meta/caps, caps
	long	C(0xf2), C(0x72)			# meta, plain

OLAB(tabs)
	long	O(st_caps_ctl_meta)
	long	C(0x93), C(0x13)			# meta/ctl, ctl
	long	C(0xd3), C(0x53)			# meta/caps, caps
	long	C(0xf3), C(0x73)			# meta, plain

OLAB(tabt)
	long	O(st_caps_ctl_meta)
	long	C(0x94), C(0x14)			# meta/ctl, ctl
	long	C(0xd4), C(0x54)			# meta/caps, caps
	long	C(0xf4), C(0x74)			# meta, plain

OLAB(tabu)
	long	O(st_caps_ctl_meta)
	long	C(0x95), C(0x15)			# meta/ctl, ctl
	long	C(0xd5), C(0x55)			# meta/caps, caps
	long	C(0xf5), C(0x75)			# meta, plain

OLAB(tabv)
	long	O(st_caps_ctl_meta)
	long	C(0x96), C(0x16)			# meta/ctl, ctl
	long	C(0xd6), C(0x56)			# meta/caps, caps
	long	C(0xf6), C(0x76)			# meta, plain

OLAB(tabw)
	long	O(st_caps_ctl_meta)
	long	C(0x97), C(0x17)			# meta/ctl, ctl
	long	C(0xd7), C(0x57) 			# meta/caps, caps
	long	C(0xf7), C(0x77)			# meta, plain

OLAB(tabx)
	long	O(st_caps_ctl_meta)
	long	C(0x98), C(0x18)			# meta/ctl, ctl
	long	C(0xd8), C(0x58)			# meta/caps, caps
	long	C(0xf8), C(0x78)			# meta, plain

OLAB(taby)
	long	O(st_caps_ctl_meta)
	long	C(0x99), C(0x19)			# meta/ctl, ctl
	long	C(0xd9), C(0x59)			# meta/caps, caps
	long	C(0xf9), C(0x79)			# meta, plain

OLAB(tabz)
	long	O(st_caps_ctl_meta)
	long	C(0x9a), C(0x1a)			# meta/ctl, ctl
	long	C(0xda), C(0x5a)			# meta/caps, caps
	long	C(0xfa), C(0x7a)			# meta, plain

OLAB(tabspc)
	long	O(st_ctl_meta)
	long	C(0x80), C(0x00)			# meta/ctl, ctl
	long	C(0xa0), C(' )				# meta, plain

OLAB(tabbq)
	long	O(st_shift_ctl_meta)
	long	C(0x80), C(0x00)			# meta/ctl, ctl
	long	C(0xfe), C(0x7e)			# meta/shift, shift
	long	C(0xe0), C(0x60)			# meta, plain

OLAB(tab1)
	long	O(st_shift_meta)
	long	C(0xa1), C('!)				# meta/shift, shift
	long	C(0xb1), C('1)				# meta, plain

OLAB(tab2)
	long	O(st_shift_ctl_meta)
	long	C(0x80), C(0x00)			# meta/ctl, ctl
	long	C(0xc0), C('@)				# meta/shift, shift
	long	C(0xb2), C('2)				# meta, plain

OLAB(tab3)
	long	O(st_shift_meta)
	long	C(0xa3), C(0x23)			# meta/shift, shift
	long	C(0xb3), C('3)				# meta, plain

OLAB(tab4)
	long	O(st_shift_meta)
	long	C(0xa4), C('$)				# meta/shift, shift
	long	C(0xb4), C('4)				# meta, plain

OLAB(tab5)
	long	O(st_shift_meta)
	long	C(0xa5), C('%)				# meta/shift, shift
	long	C(0xb5), C('5)				# meta, plain

OLAB(tab6)
	long	O(st_shift_ctl_meta)
	long	C(0x9e), C(0x1e)			# meta/ctl, ctl
	long	C(0xde), C('^)				# meta/shift, shift
	long	C(0xb6), C('6)				# meta, plain

OLAB(tab7)
	long	O(st_shift_meta)
	long	C(0xa6), C('&)				# meta/shift, shift
	long	C(0xb7), C('7)				# meta, plain

OLAB(tab8)
	long	O(st_shift_meta)
	long	C(0xaa), C('*)				# meta/shift, shift
	long	C(0xb8), C('8)				# meta, plain

OLAB(tab9)
	long	O(st_shift_meta)
	long	C(0xa8), C(0x28)			# meta/shift, shift
	long	C(0xb9), C('9)				# meta, plain

OLAB(tab0)
	long	O(st_shift_meta)
	long	C(0xa9), C(0x29)			# meta/shift, shift
	long	C(0xb0), C('0)				# meta, plain

OLAB(tabhyph)
	long	O(st_shift_ctl_meta)
	long	C(0x9f), C(0x1f)			# meta/ctl, ctl
	long	C(0xdf), C('_)				# meta/shift, shift
	long	C(0xad), C('-)				# meta, plain

OLAB(tabeq)
	long	O(st_shift_meta)
	long	C(0xab), C('+)				# meta/shift, shift
	long	C(0xbd), C('=)				# meta, plain

OLAB(tabbsl)
	long	O(st_shift_ctl_meta)
	long	C(0x9c), C(0x1c)			# meta/ctl, ctl
	long	C(0xfc), C('|)				# meta/shift, shift
	long	C(0xdc), C(0x5c)			# meta, plain

OLAB(tabtab)
	long	O(st_meta)
	long	C(0x89), C(0x09)			# meta, plain

OLAB(tabent)
	long	O(st_meta)
	long	C(0x8a), C(0x0a)			# meta, plain

OLAB(tabret)
	long	O(st_meta)
	long	C(0x8d), C(0x0d)			# meta, plain

OLAB(tabesc)
	long	O(st_meta)
	long	C(0x9b), C(0x1b)			# meta, plain

OLAB(tabdel)
	long	O(st_meta)
	long	C(0xff), C(0x7f)			# meta, plain

OLAB(tabbsp)
	long	O(st_ctl_meta)
	long	C(0xff), C(0x7f)			# meta/ctl, ctl
	long	C(0x88), C(0x08)			# meta, plain

OLAB(tabhlp)
	long	O(st_shift)
	long	S8(Hlp), S8(hlp)

OLAB(tablbr)
	long	O(st_shift_ctl_meta)
	long	C(0x9b), C(0x1b)			# meta/ctl, ctl
	long	C(0xfb), C('{)				# meta/shift, shift
	long	C(0xdb), C('[)				# meta, plain

OLAB(tabrbr)
	long	O(st_shift_ctl_meta)
	long	C(0x9d), C(0x1d)			# meta/ctl, ctl
	long	C(0xfd), C('})				# meta/shift, shift
	long	C(0xdd), C('])				# meta, plain

OLAB(tabsem)
	long	O(st_shift_meta)
	long	C(0xba), C(':)				# meta/shift, shift
	long	C(0xbb), C(';)				# meta, plain

OLAB(tabquot)
	long	O(st_shift_meta)
	long	C(0xa2), C('")				# meta/shift, shift
	long	C(0xa7), C(0x27)			# meta, plain

OLAB(tabcom)
	long	O(st_shift_meta)
	long	C(0xbc), C('<)				# meta/shift, shift
	long	C(0xac), C(0x2c)			# meta, plain

OLAB(tabper)
	long	O(st_shift_meta)
	long	C(0xbe), C('>)				# meta/shift, shift
	long	C(0xae), C('.)				# meta, plain

OLAB(tabsl)
	long	O(st_shift_ctl_meta)
	long	C(0x9f), C(0x1f)			# meta/ctl, ctl
	long	C(0xbf), C('?)				# meta/shift, shift
	long	C(0xaf), C('/)				# meta, plain

OLAB(tabf1);	long	O(st_shift_alt), NOP, S8(F1), S8(f1)
OLAB(tabf2);    long    O(st_shift_alt), NOP, S8(F2), S8(f2)
OLAB(tabf3);	long	O(st_shift_alt), NOP, S8(F3), S8(f3)
OLAB(tabf4);	long	O(st_shift_alt), NOP, S8(F4), S8(f4)
OLAB(tabf5);	long	O(st_shift_alt), NOP, S8(F5), S8(f5)
OLAB(tabf6);	long	O(st_shift_alt), NOP, S8(F6), S8(f6)
OLAB(tabf7);	long	O(st_shift_alt), NOP, S8(F7), S8(f7)
OLAB(tabf8);	long	O(st_shift_alt), NOP, S8(F8), S8(f8)
OLAB(tabf9);	long	O(st_shift_alt), NOP, S8(F9), S8(f9)
OLAB(tabf10);	long	O(st_shift_alt), NOP, S8(F10), S8(f10)

OLAB(strf1);    byte    3,0x9b,'0,'~
OLAB(strf2);	byte	3,0x9b,'1,'~
OLAB(strf3);	byte	3,0x9b,'2,'~
OLAB(strf4);	byte	3,0x9b,'3,'~
OLAB(strf5);	byte	3,0x9b,'4,'~
OLAB(strf6);	byte	3,0x9b,'5,'~
OLAB(strf7);	byte	3,0x9b,'6,'~
OLAB(strf8);	byte	3,0x9b,'7,'~
OLAB(strf9);	byte	3,0x9b,'8,'~
OLAB(strf10);	byte	3,0x9b,'9,'~

OLAB(strF1);    byte    4,0x9b,'1,'0,'~
OLAB(strF2);	byte	4,0x9b,'1,'1,'~
OLAB(strF3);	byte	4,0x9b,'1,'2,'~
OLAB(strF4);	byte	4,0x9b,'1,'3,'~
OLAB(strF5);	byte	4,0x9b,'1,'4,'~
OLAB(strF6);	byte	4,0x9b,'1,'5,'~
OLAB(strF7);	byte	4,0x9b,'1,'6,'~
OLAB(strF8);	byte	4,0x9b,'1,'7,'~
OLAB(strF9);	byte	4,0x9b,'1,'8,'~
OLAB(strF10);	byte	4,0x9b,'1,'9,'~

OLAB(strhlp)
	byte	4,'h,'e,'l,'p

OLAB(strHlp)
	byte	4,'H,'e,'l,'p

OLAB(st_amiga)
	long	0x0400, 0x0000

OLAB(st_shift_amiga)
	long	0x0500, 0x0400, 0x0000

OLAB(strua)
	byte	2,0x9b,'A

OLAB(strda)
	byte	2,0x9b,'B

OLAB(strra)
	byte	2,0x9b,0x43

OLAB(strla)
	byte	2,0x9b,'D

OLAB(tabua)
	long	O(st_shift_amiga)
	long	C(0xff00f0), C(0xff00ff), S8(ua)

OLAB(tabda)
	long	O(st_shift_amiga)
	long	C(0xff0010), C(0xff0001), S8(da)

OLAB(tabra)
	long	O(st_shift_amiga)
	long	C(0xff1000), C(0xff0100), S8(ra)

OLAB(tabla)
	long	O(st_shift_amiga)
	long	C(0xfff000), C(0xffff00), S8(la)

OLAB(tablalt)
	long	O(st_amiga)
	long	N(M0D), NOP

OLAB(tablalu)
	long	O(st_amiga)
	long	N(M0U), NOP

OLAB(tabralt)
	long	O(st_amiga)
	long	N(M2D), NOP

OLAB(tabralu)
	long	O(st_amiga)
	long	N(M2U), NOP

OLAB(KEND)
	long	0
