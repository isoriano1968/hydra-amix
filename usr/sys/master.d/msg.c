#include "sys/types.h"
#include "sys/map.h"
#include "sys/ipc.h"
#include "sys/msg.h"

#define MSGMAP  100
#define MSGMAX  2048
#define MSGMNB  4096
#define MSGMNI  50
#define MSGSSZ  8
#define MSGTQL  40
#define MSGSEG  1024

struct map msgmap[MSGMAP];
struct msqid_ds msgque[MSGMNI];
char msglock[MSGMNI];
struct msg msgh[MSGTQL];
struct msginfo msginfo
      ={MSGMAP,
	MSGMAX,
	MSGMNB,
	MSGMNI,
	MSGSSZ,
	MSGTQL,
	MSGSEG};
