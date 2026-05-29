#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/screen.h>

main()
{
    int sd;				/* Screen descriptor */
    int bd;				/* Bitmap descriptor */
    struct scrtype scrtype;
    struct bmap bmap;
    unsigned char *bitplanes[2];	/* Bitplane pointers */
    static struct scolor scolor[4] =
    {
	{ 0x00, 0xff, 0x00, 0x00, },	/* gray,red,green,blue */
	{ 0x55, 0x00, 0xff, 0x00, },	/* gray,red,green,blue */
	{ 0xaa, 0x00, 0x00, 0xff, },	/* gray,red,green,blue */
	{ 0xff, 0x00, 0x00, 0x00, },	/* gray,red,green,blue */
    };
    struct stat Stat;

    sd = open("/dev/screen", O_RDWR);
    if (sd<0 || fstat(sd, &Stat))
    {
	perror("open /dev/screen");
	return 1;
    }
    printf("Got /dev/scr/%d\n", minor(Stat.st_rdev));

    scrtype.flags = SF_LESSRES;
    scrtype.type = 0;
    scrtype.dispx = 2048;
    scrtype.dispy = 2048;
    scrtype.dispz = 2;
    scrtype.modes = SM_NONHEDLEY;
    if (ioctl(sd, SIOCSETTYPE, &scrtype))
    {
	perror("SIOCSETTYPE");
	return 1;
    }
    printf("SIOCSETTYPE done\n");

    if (ioctl(sd, SIOCGETTYPE, &scrtype))
    {
	perror("SIOCGETTYPE");
	return 1;
    }
    printf("SIOCGETTYPE reports flags=0x%x, type=%d, x=%d,y=%d,z=%d\n",
	   scrtype.flags, scrtype.type,
	   scrtype.dispx, scrtype.dispy, scrtype.dispz);

    bmap.flags = 0;
    bmap.bmapx = scrtype.dispx;
    bmap.bmapy = scrtype.dispy;
    bmap.bmapz = scrtype.dispz;
    bd = ioctl(sd, SIOCALLOCBMAP, &bmap);
    if (bd<0)
    {
	perror("first SIOCALLOCBMAP");
	return 1;
    }
    printf("SIOCALLOCBMAP done, bd=%d\n", bd);

    bitplanes[0] = (unsigned char *)mmap((caddr_t)0,
					 (bmap.bmapx * bmap.bmapy)/8,
					 PROT_READ|PROT_WRITE, MAP_SHARED,
					 sd, BPADDR(bd,0,0));
    bitplanes[1] = (unsigned char *)mmap((caddr_t)0,
					 (bmap.bmapx * bmap.bmapy)/8,
					 PROT_READ|PROT_WRITE, MAP_SHARED,
					 sd, BPADDR(bd,1,0));

    if ((bitplanes[0] == (unsigned char *)-1) ||
	(bitplanes[1] == (unsigned char *)-1))
    {
	perror("mmap() of bitplanes failed");
	return 1;
    }
    printf("mmaps done:\n\tbpl0=0x%x, bpl1=0x%x\n",
	   bitplanes[0], bitplanes[1]);

    if (ioctl(sd, SIOCSELBMAP, bd))
    {
	perror("SIOCSELBMAP");
	return 1;
    }
    printf("SIOCSELBMAP done\n");

    if (ioctl(sd, SIOCSETCMAP, scolor))
    {
	perror("SIOCSETCMAP");
	return 1;
    }
    printf("SIOCSETCMAP done\n");

    if (ioctl(sd, SIOCFRONT))
    {
	perror("SIOCFRONT");
	return 1;
    }
    printf("SIOCFRONT done\n");

    if (ioctl(sd, SIOCACTIVATE))
    {
	perror("SIOCACTIVATE");
	return 1;
    }
    printf("SIOCACTIVATE done\n");

    while (1)
    {
	extern unsigned long rand();
	bitplanes[0][rand()%(bmap.bmapx * bmap.bmapy / 8)] ^=
	    (1<<(rand()&7));
    }
    /* NOTREACHED */
}
