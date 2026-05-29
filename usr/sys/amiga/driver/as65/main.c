

#include	"as65.h"
#include	"seg.h"


static void
	longcheck( ),
	link( );


main( )
{

	yyparse( );
	longcheck( );
	segcheck( );
	link( );
	return 0;
}


static void
longcheck( )
{
	uint	s;
	bool	changed;

	do {
		changed = FALSE;
		for (s=0; segselect( s); ++s)
			if (cexpand( segcode, segaddr))
				changed = TRUE;
	} while (changed);
}


static void
link( )
{
	uint	s,
		a;

	a = 0;
	for (s=0; segselect( s); ++s) {
		while (a < segaddr) {
			putchar( 0);
			++a;
		}
		cemit( segcode, segaddr);
		a += seglen;
	}
}
