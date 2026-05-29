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
	byte	0xff, 0xff, 0xff, 0xff
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
long	T(spc),	T(bsp),	C(0x09),C(0x0a),C(0x0d),C(0x1b),C(0x7f),NOP	#40-47
long	NOP,	NOP,	C('-),	NOP,	S8(ua),	S8(da),	S8(ra),	S8(la)	#48-4f
long	T(f1),	T(f2),	T(f3),	T(f4),	T(f5),	T(f6),	T(f7),	T(f8)	#50-57
long	T(f9),	T(f10),	C(0x28),C(0x29),C('/),	C('*),	C('+),	T(hlp)	#58-5f
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#60-67
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#68-6f
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#70-77
long	NOP,	NOP,	NOP,	NOP,	N('0),	N('1),	N('2),	NOP	#78-7f
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
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#e0-e7
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#e8-ef
long	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP	#f0-f7
long	NOP,	NOP,	NOP,	NOP,	N('A),	N('B),	N(0x43),NOP	#f8-ff

OLAB(st_caps)
	long	0x0800, 0x0000

OLAB(st_shift)
	long	0x0100, 0x0000

OLAB(st_shift_alt)
	long	0x0200, 0x0100, 0x0000

OLAB(st_ctl)
	long	0x0008, 0x0000

OLAB(st_shift_ctl)
	long	0x0008, 0x0100, 0x0000

OLAB(st_caps_ctl)
	long	0x0008, 0x0800, 0x0000

OLAB(st_shift_ctl_alt)
	long	0x0200, 0x0008, 0x0100, 0x0000

OLAB(st_caps_ctl_alt)
	long	0x0200, 0x0008, 0x0800, 0x0000

OLAB(st_caps_ctl_dead)
	long	0x000008				# ctl
	long	0x100800, 0x080800, 0x040800, 0x030800, 0x010800, 0x020800
	long	0x000800
	long	0x100000, 0x080000, 0x040000, 0x030000, 0x010000, 0x020000
	long	0x000000

OLAB(taba)
	long	O(st_caps_ctl_dead)
	long	C(0x01)					# ctl
	long	C(0xc4), C(0xc3), C(0xc2), C(0xc2), C(0xc1), C(0xc0), C(0x41)
	long	C(0xe4), C(0xe3), C(0xe2), C(0xe2), C(0xe1), C(0xe0), C(0x61)

OLAB(tabb)
	long	O(st_caps_ctl)
	long	C(0x02), C(0x42), C(0x62)		# ctl, upper, lower

OLAB(tabc)
	long	O(st_caps_ctl)
	long	C(0x03), C(0x43), C(0x63)		# ctl, upper, lower

OLAB(tabd)
	long	O(st_caps_ctl)
	long	C(0x04), C(0x44), C(0x64)		# ctl, upper, lower

OLAB(tabe)
	long	O(st_caps_ctl_dead)
	long	C(0x05)					# ctl
	long	C(0xcb), C(0x45), C(0xca), C(0xca), C(0xc9), C(0xc8), C(0x45)
	long	C(0xeb), C(0x65), C(0xea), C(0xea), C(0xe9), C(0xe8), C(0x65)

OLAB(tabf)
	long	O(st_caps_ctl_alt)
	long	S(0x10000), C(0x06), C('F), C('f)	# alt, ctl, upper, lower

OLAB(tabg)
	long	O(st_caps_ctl_alt)
	long	S(0x20000), C(0x07), C('G), C('g)	# alt, ctl, upper, lower

OLAB(tabh)
	long	O(st_caps_ctl_alt)
	long	S(0x40000), C(0x08), C('H), C('h)	# alt, ctl, upper, lower

OLAB(tabi)
	long	O(st_caps_ctl_dead)
	long	C(0x09)					# ctl
	long	C(0xcf), C(0x49), C(0xce), C(0xce), C(0xcd), C(0xcc), C(0x49)
	long	C(0xef), C(0x69), C(0xee), C(0xee), C(0xed), C(0xec), C(0x69)

OLAB(tabj)
	long	O(st_caps_ctl_alt)
	long	S(0x80000), C(0x0a), C('J), C('j)	# alt, ctl, upper, lower

OLAB(tabk)
	long	O(st_caps_ctl_alt)
	long	S(0x100000), C(0x0b), C('K), C('k)	# alt, ctl, upper, lower

OLAB(tabl)
	long	O(st_caps_ctl)
	long	C(0x0c), C(0x4c), C(0x6c)		# ctl, upper, lower

OLAB(tabm)
	long	O(st_caps_ctl)
	long	C(0x0d), C(0x4d), C(0x6d)		# ctl, upper, lower

OLAB(tabn)
	long	O(st_caps_ctl)
	long	C(0x0e), C(0x4e), C(0x6e)		# ctl, upper, lower

OLAB(tabo)
	long	O(st_caps_ctl_dead)
	long	C(0x0f)					# ctl
	long	C(0xd6), C(0xd5), C(0xd4), C(0xd4), C(0xd3), C(0xd2), C(0x4f)
	long	C(0xf6), C(0xf5), C(0xf4), C(0xf4), C(0xf3), C(0xf2), C(0x6f)

OLAB(tabp)
	long	O(st_caps_ctl)
	long	C(0x10), C(0x50), C(0x70)		# ctl, upper, lower

OLAB(tabq)
	long	O(st_caps_ctl)
	long	C(0x11), C(0x51), C(0x71)		# ctl, upper, lower

OLAB(tabr)
	long	O(st_caps_ctl)
	long	C(0x12), C(0x52), C(0x72)		# ctl, upper, lower

OLAB(tabs)
	long	O(st_caps_ctl)
	long	C(0x13), C(0x53), C(0x73)		# ctl, upper, lower

OLAB(tabt)
	long	O(st_caps_ctl)
	long	C(0x14), C(0x54), C(0x74)		# ctl, upper, lower

OLAB(tabu)
	long	O(st_caps_ctl_dead)
	long	C(0x15)					# ctl
	long	C(0xdc), C(0x55), C(0xdb), C(0xdb), C(0xda), C(0xd9), C(0x55)
	long	C(0xfc), C(0x75), C(0xfb), C(0xfb), C(0xfa), C(0xf9), C(0x75)

OLAB(tabv)
	long	O(st_caps_ctl)
	long	C(0x16), C(0x56), C(0x76)		# ctl, upper, lower

OLAB(tabw)
	long	O(st_caps_ctl)
	long	C(0x17), C(0x57), C(0x77)		# ctl, upper, lower

OLAB(tabx)
	long	O(st_caps_ctl)
	long	C(0x18), C(0x58), C(0x78)		# ctl, upper, lower

OLAB(taby)
	long	O(st_caps_ctl_dead)
	long	C(0x19)					# ctl
	long	C(0x59), C(0x59), C(0x59), C(0x59), C(0xdd), C(0x59), C(0x59)
	long	C(0xff), C(0x79), C(0x79), C(0x79), C(0xfd), C(0x79), C(0x79)

OLAB(tabz)
	long	O(st_caps_ctl)
	long	C(0x1a), C(0x5a), C(0x7a)		# ctl, upper, lower

OLAB(tabspc)
	long	O(st_shift_ctl)
	long	C(0x00), C(' ), C(' )

OLAB(tabbq)
	long	O(st_shift_ctl)
	long	C(0x00), C('~), C(0x60)

OLAB(tab1)
	long	O(st_shift)
	long	C(0x21), C('1)

OLAB(tab2)
	long	O(st_shift_ctl)
	long	C(0x00), C('@), C('2)

OLAB(tab3)
	long	O(st_shift)
	long	C(0x23), C('3)

OLAB(tab4)
	long	O(st_shift)
	long	C('$), C('4)

OLAB(tab5)
	long	O(st_shift)
	long	C('%), C('5)

OLAB(tab6)
	long	O(st_shift_ctl)
	long	C(0x1e), C('^), C('6)

OLAB(tab7)
	long	O(st_shift)
	long	C('&), C('7)

OLAB(tab8)
	long	O(st_shift)
	long	C('*), C('8)

OLAB(tab9)
	long	O(st_shift)
	long	C(0x28), C('9)

OLAB(tab0)
	long	O(st_shift)
	long	C(0x29), C('0)

OLAB(tabhyph)
	long	O(st_shift_ctl)
	long	C(0x1f), C('_), C('-)

OLAB(tabeq)
	long	O(st_shift)
	long	C('+), C('=)

OLAB(tabbsl)
	long	O(st_shift_ctl)
	long	C(0x1c), C('|), C(0x5c)

OLAB(tabbsp)
	long	O(st_ctl)
	long	C(0x7f), C(0x08)

OLAB(tabhlp)
	long	O(st_shift)
	long	S8(Hlp), S8(hlp)

OLAB(tablbr)
	long	O(st_shift_ctl)
	long	C(0x1b), C('{), C('[)

OLAB(tabrbr)
	long	O(st_shift_ctl)
	long	C(0x1d), C('}), C('])

OLAB(tabsem)
	long	O(st_shift)
	long	C(':), C(';)

OLAB(tabquot)
	long	O(st_shift)
	long	C('"), C(0x27)

OLAB(tabcom)
	long	O(st_shift)
	long	C('<), C(0x2c)

OLAB(tabper)
	long	O(st_shift)
	long	C('>), C('.)

OLAB(tabsl)
	long	O(st_shift_ctl)
	long	C(0x1f), C('?), C('/)

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

OLAB(strua)
	byte	2,0x9b,'A

OLAB(strda)
	byte	2,0x9b,'B

OLAB(strra)
	byte	2,0x9b,0x43

OLAB(strla)
	byte	2,0x9b,'D

OLAB(strhlp)
	byte	4,'h,'e,'l,'p

OLAB(strHlp)
	byte	4,'H,'e,'l,'p

OLAB(KEND)
	long	0
