
#define NAMENFS	"nfs"
#define NRNODE	300
/* transfer sizes - must be large enough for NFS_MAXDATA + RPC header */
#define NFSSVCSENDSZ	8800
#define NFSCLTSENDSZ	8800

char nfsname[15]= { NAMENFS };
long nrnode = { NRNODE };
int nfssvcsendsz = {NFSSVCSENDSZ};
int nfscltsendsz = {NFSCLTSENDSZ};
