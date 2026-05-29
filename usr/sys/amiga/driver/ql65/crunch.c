

/*
 * squeeze nul characters
 * A cheap way to compress a stream of bytes when many contiguous nuls are
 * expected.  Use in conjunction with `uncrunch'.
 */

#include	<stdio.h>
#include	"rico.h"


#define	BREAKEVEN	(2 * sizeof( short))


main( )
{
	int	nulls,
		c;

	nulls = 0;
	loop
		switch (c = getchar( )) {
		case EOF:
			appendnulls( nulls);
			flushc( );
			return;
		case '\0':
			++nulls;
			break;
		default:
			appendnulls( nulls);
			nulls = 0;
			stashc( c);
		}
}


appendnulls( nulls)
{

	if (nulls >= BREAKEVEN) {
		flushc( );
		putshort( -nulls);
	}
	else
		while (nulls) {
			stashc( '\0');
			--nulls;
		}
}


static char	obuffer[1024];
static		ocount;


stashc( c)
{

	obuffer[ocount++] = c;
	if (ocount == sizeof obuffer)
		flushc( );
}


flushc( )
{

	if (ocount) {
		putshort( ocount);
		fwrite( obuffer, ocount, 1, stdout);
		ocount = 0;
	}
}


putshort( s)
short	s;
{

	fwrite( &s, sizeof s, 1, stdout);
}
