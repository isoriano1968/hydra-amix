
define(structure,`define(`structname','$1`)set structoff,'$2)
define(BYTE,set $1`,'structoff;set structoff`,'structoff+`ifelse($#,1,1,$2)')
define(WORD,set $1`,'structoff;set structoff`,'structoff+`ifelse($#,1,2,(2*$2))')
define(LONG,set $1`,'structoff;set structoff`,'structoff+`ifelse($#,1,4,(4*$2))')
define(STRUCT,set $2`,'structoff;set structoff`,'structoff+`ifelse($#,2,`sizeof_'$1,(`sizeof_'$1*$3))')
define(endstruct,set `sizeof_`''structname`,'structoff)


#
# FONT INFORMATION
#
# WARNING: the layouts of (struct charent) and (struct font) are also
# defined in screen.h, which must always be kept in sync with these
# definitions.
#

structure(charent,0)
	BYTE(ce_height)
	BYTE(ce_width)
	BYTE(ce_basel)
	BYTE(ce_move)
	BYTE(ce_bpl)
	BYTE(ce_pad)
	WORD(ce_data)
endstruct

structure(font,0)
	WORD(f_magic)
	WORD(f_flags)
	WORD(f_count)
	WORD(f_pad0)
	LONG(f_length)
	BYTE(f_height)
	BYTE(f_width)
	BYTE(f_baseline)
	BYTE(f_pad1)
	STRUCT(charent,f_ctable,256)
endstruct

# font.f_magic
define(F_MAGIC,0x2a46)


#
# BITMAP INFORMATION
#
# WARNING: the layout of (struct bitmap) is also defined in screen.h,
# which must always be kept in sync with these definitions.
#

define(MAXBPL,8)			# Max bitplanes in a bitmap

define(Bf_ACTIVE,0x01)
define(Bf_MAPPED,0x02)

structure(bitmap,0)
	WORD(bm_flags)
	WORD(bm_width)
	WORD(bm_height)
	WORD(bm_depth)
	WORD(bm_offset)
	WORD(bm_pad0)
	LONG(bm_bpl,MAXBPL)
endstruct
