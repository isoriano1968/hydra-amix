/*
 * rigid disk block
 *
 * AmigaUNIX uses the AmigaDOS disk partitioning standard.
 * The hardblocks.h headerfile from AmigaDOS is used unchanged.
 *
 */

typedef unsigned long ULONG;
typedef long           LONG;
typedef unsigned char UBYTE;
#define EXEC_TYPES_H
#include <sys/amiga/hardblocks.h>
#include <sys/amiga/env.h>
