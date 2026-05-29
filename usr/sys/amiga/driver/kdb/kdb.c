#include <sys/types.h>
#include <sys/wait.h>
#include <sys/procfs.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/psw.h>
#include <sys/pcb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <nlist.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <sys/sockio.h>
#include <sys/stream.h>
#include <stropts.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/amiga/kdebug.h>

extern errno;

char *progname;

void usage();
int doioctl();

main(argc, argv)
int argc;
char *argv[];
{
	int aenfd, kdbgfd, it, n;
	ether_addr_t eaddr;

	progname = *argv++; --argc;

	if (argc != 1)
	{
		usage();
	}

	if (!geteaddr(*argv, eaddr))
	{
		fprintf(stderr, "%s: invalid argument: %s\n", progname, *argv);
		usage();
	}

	ether_print(eaddr);

	if ((aenfd = open("/dev/aen0", O_RDWR)) == -1)
	{
		perror("open /dev/aen0 failed");
		return 1;
	}

	if ((it = ioctl(aenfd, I_FIND, "rkmod")) == -1)
	{
		perror("ioctl I_FIND rkmod");
		return 1;
	}

	if (it == 0)
	{
		if (ioctl(aenfd, I_PUSH, "rkmod"))
		{
			perror ("ioctl I_PUSH rkmod");
			return 1;
		}
	}

	if ((kdbgfd = open("/dev/rkdb", O_RDWR)) == -1)
	{
		perror("open /dev/rkdb failed");
		return 1;
	}

	if (ioctl(kdbgfd, KDB_SET_ETHERNET_ADDRESS, eaddr))
	{
		perror("ioctl on /dev/rkdb to set ethernet address failed");
		return 1;
	}

	if ((n = fork()) == -1)
	{
		perror("couldn't fork");
		return 1;
	}
	if (!n)
	{
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		pause();
	}

	return 0;
}

void
usage()
{
	fprintf(stderr,
		"usage: %s { hostname | hostnumber | ethernet_address }\n",
		 progname);
	exit(1);
}

geteaddr(host, eaddr)
char *host;
ether_addr_t eaddr;
{
	struct arpreq ar;
	struct hostent *hp;
	struct sockaddr_in *sin;
	int s;

	memset((caddr_t)&ar, 0, sizeof ar);
	if (isdigit(host[0]))
	{
		char *p = eaddr;
		int i;
		/* this is probably an ethernet address */
		for (i=0; i<6; ++i)
		{
			int num;

			if (!isxdigit(*host))
			{
				fprintf(stderr,
					 "%s: Invalid ethernet address\n",
					progname);
				exit(1);
			}
			num = *host <= '9' ?
				 *host - '0' : toupper(*host) - 'A' + 10;
			host++;
			if (isxdigit(*host))
			{
				num *= 0x10;
				num += *host <= '9' ?
				 *host - '0' : toupper(*host) - 'A' + 10;
				host++;
			}
			host++;
			*p++ = num;
		}
		return 1;
	}
	ar.arp_pa.sa_family = AF_INET;
	sin = (struct sockaddr_in *)&ar.arp_pa;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = inet_addr(host);

	if (sin->sin_addr.s_addr == -1)
	{
		if ((hp = gethostbyname(host)) == NULL)
		{
			/* <IMPLEMENT> check to see if they gave an eth addr*/
			fprintf(stderr, "%s: couldn't find host: %s\n",
				progname, host);
			return 0;
		}
		memcpy((char *)&sin->sin_addr, (char *)hp->h_addr,
		    sizeof sin->sin_addr);
	}

	if (doioctl(SIOCGARP, (caddr_t)&ar) == -1)
	{
		if (errno == ENXIO)
		{
			if (!pinghost(host) || doioctl(SIOCGARP, (caddr_t)&ar))
			{
				fprintf(stderr, "%s: couldn't SIOCGARP (%s)\n",
					progname, strerror(errno));
				return 0;
			}
		}
		else
		{
			fprintf(stderr, "%s: couldn't SIOCGARP (%s)\n",
				progname, strerror(errno));
			return 0;
		}
	}

	memcpy((char *)eaddr, (char *)ar.arp_ha.sa_data, sizeof(ether_addr_t));
	return 1;
}

int
doioctl (name, arg)
int name;
caddr_t arg;
{
	int d;
	struct strioctl sti;
	int ret;

	if ((d = open ("/dev/arp", O_RDWR)) == -1)
	{
		fprintf(stderr, "%s: open /dev/arp failed (%s)\n", progname,
			strerror(errno));
                exit(1);
        }
	sti.ic_cmd = name;
	sti.ic_timout = 0;
	sti.ic_len = sizeof(struct arpreq);
	sti.ic_dp = arg;
	ret = ioctl(d, I_STR, (caddr_t)&sti);
	close(d);
	return (ret);
}

ether_print(cp)
	u_char *cp;
{
	printf("Address to debug: %x:%x:%x:%x:%x:%x\n", cp[0], cp[1], cp[2],
		 cp[3], cp[4], cp[5]);
}

pinghost(host)
char *host;
{
	int pid, status, retval;

	if ((pid = fork()) < 0)
	{
		fprintf(stderr, "%s: fork failed (%s)\n", progname,
				 strerror(errno));
		exit(1);
	}
	if (pid)
	{
		do
		{
			if ((retval = wait(&status)) == -1)
			{
				fprintf(stderr, "%s: wait failed (%s)\n",
					 progname, strerror(errno));
				exit(1);
			}
	
		} while (retval != pid);
	}
	else
	{
		execlp("ping", "ping", host, (char *)0);
		fprintf(stderr, "%s: exec of ping failed (%s)\n", progname,
				 strerror(errno));
		exit(1);
	}
}
