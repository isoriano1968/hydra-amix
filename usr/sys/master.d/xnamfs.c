#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/signal.h"
#include "sys/user.h"
#include "sys/file.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/proc.h"
#include "sys/fs/xnamnode.h"
#include "sys/sd.h"

#define NAMEXNAM  "xnamfs"
#define XSEMMAX  1
#define XSDSEGS  1
#define XSDSLOTS  1

char xnamname[15]= { NAMEXNAM };
struct xsem xsem[XSEMMAX];
int nxsem ={XSEMMAX};
struct xsd xsd[XSDSEGS];
struct sd sdtab[XSDSEGS * XSDSLOTS];
int nxsd ={XSDSEGS};
