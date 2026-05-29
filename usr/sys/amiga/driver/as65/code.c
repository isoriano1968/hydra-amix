

#include	"as65.h"
#include	"expr.h"
#include	"seg.h"

#define hibyte(x) ((x)>>8)

struct instr {
	struct instr	*next;
	uint		lineno,
			type,
			opcode,
			opcode2;
	struct expr	*expr;
};
/* instr.type
 */
#define	NO_ARG	0
#define	SHORT	1
#define	BRANCH	2
#define	LONG	3
#define	VARIES	4
#define	DATA1	5
#define	DATA2	6


static uint isize[] = {
	1, 2, 2, 3, 2, 1, 2
};
static struct instr	*lastp;

struct instr	*newinstr( );


cselect( )
{
	struct instr	*p;

	for (p=segcode; p; p=p->next)
		lastp = p;
}


cgen1( opcode)
{

	newinstr( NO_ARG)->opcode = opcode;
}


cgen2( opcode, expr)
struct expr	*expr;
{
	struct instr	*p;

	p = newinstr( SHORT);
	p->expr = expr;
	p->opcode = opcode;
}


cgen2r( opcode, expr)
struct expr	*expr;
{
	struct instr	*p;

	p = newinstr( BRANCH);
	p->expr = expr;
	p->opcode = opcode;
}


cgen3( opcode, expr)
struct expr	*expr;
{
	struct instr	*p;

	p = newinstr( LONG);
	p->expr = expr;
	p->opcode = opcode;
}


cgenx( opcode, opcode2, expr)
struct expr	*expr;
{
	struct instr	*p;

	p = newinstr( VARIES);
	p->expr = expr;
	p->opcode = opcode;
	p->opcode2 = opcode2;
}


cdata1( expr)
struct expr	*expr;
{

	newinstr( DATA1)->expr = expr;
}


cdata2( expr)
struct expr	*expr;
{

	newinstr( DATA2)->expr = expr;
}


cblock( expr)
struct expr	*expr;
{
	uint	i;

	if (not eabsolute( expr))
		croak( "absolute required, line %d", lineno);
	i = evaluate( expr);
	while (i--)
		newinstr( NO_ARG)->opcode = 0;
}


bool
cexpand( ip, addr)
struct instr	*ip;
uint		addr;
{
	bool	changed;
	int	i;

	changed = FALSE;
	while (ip) {
		lineno = ip->lineno;
		if ((ip->type == VARIES)
		and (evaluate( ip->expr) > 0xFF)) {
			ip->type = LONG;
			ip->opcode = ip->opcode2;
			i = isize[ip->type] - isize[VARIES];
			sadjust( addr, i);
			seglen += i;
			changed = TRUE;
		}
		addr += isize[ip->type];
		ip = ip->next;
	}
	return (changed);
}


cemit( ip, addr)
struct instr	*ip;
uint		addr;
{
	int	i;

	while (ip) {
		lineno = ip->lineno;
		addr += isize[ip->type];
		switch (ip->type) {
		case NO_ARG:
			putchar( ip->opcode);
			break;
		case SHORT:
		case VARIES:
			putchar( ip->opcode);
			i = evaluate( ip->expr);
			if ((i < -128)
			or (i > 255))
				croak( "value too big, line %d", lineno);
			putchar( i);
			break;
		case BRANCH:
			putchar( ip->opcode);
			i = evaluate( ip->expr) - addr;
			if ((i < -128)
			or (i > 127))
				croak( "branch too far, line %d", lineno);
			putchar( i);
			break;
		case LONG:
			putchar( ip->opcode);
			i = evaluate( ip->expr);
			putchar( i);
			putchar( hibyte( i));
			break;
		case DATA1:
			i = evaluate( ip->expr);
			if ((i < -128)
			or (i > 255))
				croak( "value too big, line %d", lineno);
			putchar( i);
			break;
		case DATA2:
			i = evaluate( ip->expr);
			putchar( i);
			putchar( hibyte( i));
		}
		ip = ip->next;
	}
}


static struct instr	*
newinstr( type)
{
	struct instr	*p;

	p = malloc( sizeof *p);
	if (not p)
		nomem( );
	if (not segcode)
		segcode = p;
	else
		lastp->next = p;
	lastp = p;
	p->next = 0;
	p->lineno = lineno;
	p->type = type;
	seglen += isize[type];
	return (p);
}
