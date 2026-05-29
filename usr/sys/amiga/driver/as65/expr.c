

#include	"as65.h"
#include	"expr.h"


static void
	etoobig( );


/* expr.type
 */
#define	FUNC	0
#define	SYM	1
#define	CONST	2
#define	END	3


static struct expr	ebuf[20];
static uint		emax;


efunction( f)
uint	f;
{

	ebuf[emax].type = FUNC;
	ebuf[emax].value = f;
	if (++emax == nel( ebuf))
		etoobig( );
}


esymbol( s)
uint	s;
{

	ebuf[emax].type = SYM;
	ebuf[emax].value = s;
	if (++emax == nel( ebuf))
		etoobig( );
}


econstant( c)
uint	c;
{

	ebuf[emax].type = CONST;
	ebuf[emax].value = c;
	if (++emax == nel( ebuf))
		etoobig( );

}


struct expr	*
expression( )
{
	struct expr	*p;

	ebuf[emax++].type = END;
	p = malloc( emax*sizeof( *p));
	if (not p)
		nomem( );
	memcpy( p, ebuf, emax*sizeof( *p));
	emax = 0;
	return (p);
}


uint
evaluate( p)
struct expr	*p;
{
	uint	stack[5];
	uint	si;

	si = 0;
	for (; ; ++p)
		switch (p->type) {
		case FUNC:
			switch (p->value) {
			case '+':
				--si;
				stack[si-1] += stack[si];
				break;
			case '-':
				--si;
				stack[si-1] -= stack[si];
				break;
			case '*':
				--si;
				stack[si-1] *= stack[si];
				break;
			case '/':
				--si;
				stack[si-1] /= stack[si];
				break;
			case 'n':
				stack[si-1] = -stack[si-1];
				break;
			case '<<':
				--si;
				stack[si-1] <<= stack[si];
				break;
			case '>>':
				--si;
				stack[si-1] >>= stack[si];
			}
			break;
		case SYM:
			stack[si++] = svalue( p->value);
			break;
		case CONST:
			stack[si++] = p->value;
			break;
		case END:
			return (stack[si-1]);
		}
}


bool
eabsolute( p)
struct expr	*p;
{

	for (; ; ++p)
		switch (p->type) {
		case SYM:
			return (FALSE);
		case END:
			return (TRUE);
		}
}


static void
etoobig( )
{

	croak( "expression too big, line %d", lineno);
}
