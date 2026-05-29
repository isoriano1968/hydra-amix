#include "sys/types.h"
#include "sys/ipc.h"
#include "sys/map.h"
#include "sys/sem.h"
#include "kernel.h"	/* for NPROC */

#define max(a,b)	((a)<(b)?(b):(a))

#define	NBPW	4
#define SEMMAP	10
#define SEMMNI	10
#define SEMMNS	60
#define SEMMNU	30
#define SEMMSL	25
#define SEMOPM	10
#define SEMUME	10
#define SEMVMX	32767
#define SEMAEM	16384

struct semid_ds sema[SEMMNI];
struct sem sem[SEMMNS];
struct map semmap[SEMMAP];
struct sem_undo *sem_undo[NPROC];
int semu[((16+8*SEMUME)*SEMMNU+NBPW-1)/NBPW];	/* not quite right? */
union{
	ushort 		a[1];
	struct semid_ds b;
	struct sembuf   c[1];
} semtmp[max(2*SEMMSL,max(0x20,8*SEMOPM))];	/* ??? */

struct seminfo seminfo
      ={SEMMAP,
	SEMMNI,
	SEMMNS,
	SEMMNU,
	SEMMSL,
	SEMOPM,
	SEMUME,
	16+8*SEMUME,
	SEMVMX,
	SEMAEM};

