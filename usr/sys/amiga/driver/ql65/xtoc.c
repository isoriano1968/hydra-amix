

#include	<stdio.h>
#include	"rico.h"


main( argc, argv)
char	*argv[];
{
	uint	i;
	int	c;

	if (not argv[1]) {
		fprintf( stderr, "usage: xtoc identifier\n");
		return (2);
	}
	printf( "\n");
	printf( "\n");
	printf( "static unsigned char %s[] = {\n", argv[1]);
	i = 0;
	while ((c=getchar( )) != EOF) {
		printf( i? " ": "\t");
		printf( "0x%02X,", c);
		if (++i == 8) {
			printf( "\n");
			i = 0;
		}
	}
	if (i)
		printf( "\n");
	printf( "};\n");
	return (0);
}
