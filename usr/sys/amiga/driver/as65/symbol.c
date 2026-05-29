

#include	"as65.h"
#include	"seg.h"


struct sym {
	struct sym	*next;
	char		*name;
	bool		defined;
	uint		seg,
			value,
			id;
};


static struct sym	*symbols;
static uint		symid;


uint
srefer( name)
char	*name;
{
	struct sym	*p;

	for (p=symbols; p; p=p->next)
		if ((p->name)
		and (streq( p->name, name)))
			return (p->id);
	p = malloc( sizeof *p);
	if (not p)
		nomem( );
	p->name = malloc( strlen( name)+1);
	if (not p->name)
		nomem( );
	strcpy( p->name, name);
	p->defined = FALSE;
	p->next = symbols;
	symbols = p;
	return (p->id = ++symid);
}


uint
sreflocal( )
{
	struct sym	*p;

	p = malloc( sizeof *p);
	if (not p)
		nomem( );
	p->name = 0;
	p->defined = FALSE;
	p->next = symbols;
	symbols = p;
	return (p->id = ++symid);
}


void
sdefine( sid)
uint	sid;
{
	struct sym	*p;

	p = symbols;
	while (p->id != sid)
		 p = p->next;
	if (not p->name)
		croak( "incorrect use of local label, line %d", lineno);
	if (p->defined)
		croak( "\"%s\" multiply defined, line %d", p->name, lineno);
	p->defined = TRUE;
	p->seg = segment;
	p->value = segaddr + seglen;
}


void
sdeflocal( i)
uint	i;
{
	struct sym	*p;

	p = symbols;
	while (p->id != i)
		 p = p->next;
	p->defined = TRUE;
	p->seg = segment;
	p->value = segaddr + seglen;
}


uint
svalue( sid)
uint	sid;
{
	struct sym	*p;

	for (p=symbols; p; p=p->next)
		if (p->id == sid)
			if (p->defined)
				return (p->value);
			else if (p->name)
				croak( "undefined symbol \"%s\", line %d", p->name, lineno);
			else
				croak( "undefined local, line %d", lineno);
	abort( );
}


sadjust( value, incr)
uint	value;
{
	struct sym	*p;

	for (p=symbols; p; p=p->next)
		if (p->seg == segment)
			if (not p->defined)
				abort( );
			else if (p->value > value)
				p->value += incr;
}
