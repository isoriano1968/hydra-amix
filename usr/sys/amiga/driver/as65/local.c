

#include	"as65.h"


static uint	btab[10],
		ftab[10];


uint
lrefer( l, direction)
uint	l;
{

	switch (direction) {
	case 'b':
		if (not btab[l])
			croak( "undefined local label, line %d", lineno);
		return (btab[l]);
	case 'f':
		if (not ftab[l])
			ftab[l] = sreflocal( );
		return (ftab[l]);
	}
	abort( );
}


void
ldefine( l)
uint	l;
{

	if (l >= 10)
		croak( "bad local label, line %d", lineno);
	sdeflocal( lrefer( l, 'f'));
	btab[l] = ftab[l];
	ftab[l] = 0;
}
