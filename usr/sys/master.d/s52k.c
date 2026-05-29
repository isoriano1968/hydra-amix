#include "sys/types.h"
#include "sys/buf.h"

#define	NF2KBUF   100
#define NHF2KBUF  64
#define FLAGF2K   0
#define NAMEF2K  "S52K"
#define NOTFYF2K  0

struct buf s52kbuf[NF2KBUF];
char s52kbuffers[(NF2KBUF+1)*2048];
int s52kfreecnt ={NF2KBUF};
struct hbuf s52khbuf[NHF2KBUF];
int s52khash ={NHF2KBUF};
int s52kflag ={FLAGF2K};
char s52kname[15]= {NAMEF2K};
int s52knotfy = {NOTFYF2K};
