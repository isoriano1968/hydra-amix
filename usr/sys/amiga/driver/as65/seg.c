

#include	"as65.h"
#include	"seg.h"
#include	"code.h"


static void
	selectseg( ),
	flushseg( );


struct seg {
	uint		addr,
			len,
			lineno;
	struct instr	*code;
};


uint		segment,
		segaddr,
		seglen;
struct instr	*segcode;

static struct seg	segtab[9];
static uint		maxseg		= 1;

int	compseg( );


void
segcreate( expr)
struct expr	*expr;
{

	flushseg( );
	if (not eabsolute( expr))
		croak( "segment base must be absolute, line %d", lineno);
	if (maxseg == nel( segtab))
		croak( "too many segments, line %d", lineno);
	segment = maxseg++;
	segtab[segment].addr = evaluate( expr);
	segtab[segment].lineno = lineno;
	selectseg( );
}


bool
segselect( s)
uint	s;
{

	if (s >= maxseg)
		return (FALSE);
	flushseg( );
	segment = s;
	selectseg( );
	return (TRUE);
}


void
segcheck( )
{
	uint	i;

	flushseg( );
	qsort( segtab, maxseg, sizeof segtab[0], compseg);
	for (i=1; i<maxseg; ++i)
		if (segtab[i-1].addr+segtab[i-1].len > segtab[i].addr)
			croak( "overlapping segments, lines %d and %d", segtab[i-1].lineno, segtab[i].lineno);
	selectseg( );
}


static void
selectseg( )
{

	segaddr = segtab[segment].addr;
	seglen = segtab[segment].len;
	segcode = segtab[segment].code;
	cselect( );
}


static void
flushseg( )
{

	segtab[segment].len = seglen;
	segtab[segment].code = segcode;
}


static
compseg( p0, p1)
struct seg	*p0,
		*p1;
{

	if (p0->addr == p1->addr)
		return (p0->len - p1->len);
	return (p0->addr - p1->addr);
}
