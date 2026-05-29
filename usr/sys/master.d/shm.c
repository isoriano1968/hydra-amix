#include "sys/types.h"
#include "sys/ipc.h"
#include "sys/map.h"
#include "sys/shm.h"

#define SHMMAX	0x50000
#define SHMMIN	1
#define SHMMNI	100
#define SHMSEG	6

struct shmid_ds shmem[SHMMNI];
struct map shmmap[SHMMNI+1];
struct shminfo shminfo
      ={SHMMAX,
	SHMMIN,
	SHMMNI,
	SHMSEG};
