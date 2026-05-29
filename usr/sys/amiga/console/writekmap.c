#include "screen.h"

extern struct keymap builtinkmap;
extern void perror();

main()
{
    if (write(1, &builtinkmap, (unsigned)builtinkmap.km_length) !=
	builtinkmap.km_length)
    {
	perror("keymap write error");
	return 1;
    }
    return 0;
}
