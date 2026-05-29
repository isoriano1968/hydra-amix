#include "sys/types.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/conf.h"
#include "sys/sad.h"

/*
 * hash list size
 * MUST be a power of 2
 */
#define NSTRPHASH  64

/*
 * maximum number of devices to use
 * autopush with
 */
#define NAUTOPUSH  32
#define SAD_CNT  100

struct saddev saddev[SAD_CNT];
int sadcnt = SAD_CNT;
struct autopush *strpcache[NSTRPHASH];
int nstrphash = {NSTRPHASH};
int strpmask = {NSTRPHASH - 1};
struct autopush autopush[NAUTOPUSH];
int nautopush  = {NAUTOPUSH};
