

#include	"as65.h"


nomem( )
{

	croak( "out of mem");
}


croak( mesg, arg0, arg1)
char	*mesg;
{

	fflush( stdout);
	fprintf( stderr, "as65: ");
	fprintf( stderr, mesg, arg0, arg1);
	fprintf( stderr, "\n");
	exit( 1);
}
