

#include	<string.h>
#include	<stdio.h>
#include	"rico.h"


extern uint	lineno;

#ifdef __STDC__
extern void	*malloc( unsigned),
		*realloc( void *, unsigned);
#else
char	*malloc( ),
	*realloc( );
#endif
