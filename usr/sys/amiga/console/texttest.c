#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#define INKERNEL
#include <sys/screen.h>


extern unsigned long rand();
extern void putch();


#define length(array) ((sizeof (array))/(sizeof (array[0])))

#ifdef CUSTOMFONT

struct font myfont = 
{
    F_MAGIC,
    0,
    0,
    0,
    12345,
    0, 0,
    0,
    0,
    {
	0, 0,
	0, 0,
	4,
	0,
	0,
    },
};

#if OLDSYMBOL
unsigned long data[] = 
{
    0x0fff,
    0x0c03,
    0x0a05,
    0x0909,
    0x0891,
    0x0861,
    0x0891,
    0x0909,
    0x0a05,
    0x0c03,
    0x0fff,
};
#define WIDTH	12
#define HEIGHT	(length(data))
#else /* newsymbol */
unsigned long data[] = 
{
    0x00000000,		/* ................................ */
    0x0000007e,		/* .........................XXXXXX. */
    0x000000fc,		/* ........................XXXXXX.. */
    0x000001f8,		/* .......................XXXXXX... */
    0x000003f0,		/* ......................XXXXXX.... */
    0x000007e0,		/* .....................XXXXXX..... */
    0x00000fc0,		/* ....................XXXXXX...... */
    0x00001f80,		/* ...................XXXXXX....... */
    0x00003f00,		/* ..................XXXXXX........ */
    0x00007e00,		/* .................XXXXXX......... */
    0x0000fc00,		/* ................XXXXXX.......... */
    0x7e01f800,		/* .XXXXXX........XXXXXX........... */
    0x3f03f000,		/* ..XXXXXX......XXXXXX............ */
    0x1f87e000,		/* ...XXXXXX....XXXXXX............. */
    0x0fcfc000,		/* ....XXXXXX..XXXXXX.............. */
    0x07ff8000,		/* .....XXXXXXXXXXXX............... */
    0x03ff0000,		/* ......XXXXXXXXXX................ */
    0x01fe0000,		/* .......XXXXXXXX................. */
    0x00000000,		/* ................................ */
};
#define WIDTH	32
#define HEIGHT	(length(data))
#endif /* newsymbol */


struct font *builtinfont[4] = 
{
    &myfont,
    &myfont,
    &myfont,
    &myfont,
};

#define TESTCHAR 0

#else

#define WIDTH builtinfont[0]
#define TESTCHAR (' '+rand()%96)

#endif /* no customfont */



void randtest(bmap, font)
struct bitmap *bmap;
struct font *font;
{
    register int xpos, ypos;

    while (1)
    {
	register unsigned char c=TESTCHAR;
	xpos = rand()%(bmap->width-font->f_ctable[c].ce_width+1);
	ypos = rand()%(bmap->height-font->f_ctable[c].ce_height+1);
	putch(bmap, xpos, ypos, font, c);
    }
}


void testit(bmap, font)
struct bitmap *bmap;
struct font *font;
{
    register int xpos, ypos;
    register i;
    register unsigned char c=TESTCHAR;

    xpos = rand()%(bmap->width-font->f_ctable[c].ce_width+1);
    ypos = rand()%(bmap->height-font->f_ctable[c].ce_height+1);
    for ( i=0 ; i<100000 ; ++i )
	putch(bmap, xpos, ypos, font, c);
}


main(argc, argv)
int argc;
char *argv[];
{
    int sd, bd;
    struct scrtype scrtype;
    struct bitmap bitmap;
    struct bmap bmap;
    struct mapbmap mapbmap;
    static struct scolor scolor[2] =
    {
	{ 0x00, 0x00, 0x00, 0x00, },
	{ 0xff, 0xff, 0xff, 0xff, },
    };
    struct stat Stat;

#ifdef CUSTOMFONT
    myfont.f_height = HEIGHT;
    myfont.f_width = WIDTH;
    myfont.f_baseline = HEIGHT;
    myfont.f_height = HEIGHT;
    myfont.f_ctable[0].ce_height = HEIGHT;
    myfont.f_ctable[0].ce_width = WIDTH;
    myfont.f_ctable[0].ce_basel = HEIGHT;
    myfont.f_ctable[0].ce_move = WIDTH;
    myfont.f_ctable[0].ce_data =
	(unsigned short)((char *)data - (char *)&myfont);
#endif

    sd = open("/dev/screen", O_RDWR);
    if (sd<0 || fstat(sd, &Stat))
    {
	perror("open /dev/screen");
	return 1;
    }
    printf("Got screen #%d\n", minor(Stat.st_rdev));

    scrtype.flags = SF_LESSRES;
    scrtype.type = 0;
    scrtype.dispx = 2048;
    scrtype.dispy = 2048;
    scrtype.dispz = 1;
    scrtype.modes = 0/*|SM_NONLACE|SM_NONHEDLEY*/;
    if (ioctl(sd, SIOCSETTYPE, &scrtype))
    {
	perror("SIOCSETTYPE");
	return 1;
    }

    if (ioctl(sd, SIOCGETTYPE, &scrtype))
    {
	perror("SIOCGETTYPE");
	return 1;
    }

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

    mapbmap.mapnum = bd;
    mapbmap.bpl[0].virtaddr = (unsigned char *)-1;
    mapbmap.bpl[0].nbytes   = (bmap.bmapx * bmap.bmapy)/8;
    if (ioctl(sd, SIOCMAPBMAP, &mapbmap))
    {
	perror("SIOCMAPBMAP");
	return 1;
    }

    if (ioctl(sd, SIOCSELBMAP, bd))
    {
	perror("SIOCSELBMAP");
	return 1;
    }

    if (ioctl(sd, SIOCSETCMAP, scolor))
    {
	perror("SIOCSETCMAP");
	return 1;
    }

    if (ioctl(sd, SIOCFRONT))
    {
	perror("SIOCFRONT");
	return 1;
    }

    if (ioctl(sd, SIOCACTIVATE))
    {
	perror("SIOCACTIVATE");
	return 1;
    }

    bitmap.width = bmap.bmapx;
    bitmap.height = bmap.bmapy;
    bitmap.depth = bmap.bmapz;
    bitmap.offset = 0;
    bitmap.flags = 0;
    bitmap.bpl[0] = mapbmap.bpl[0].virtaddr;

    (argc>1?testit:randtest)(&bitmap, builtinfont[0]);
}
