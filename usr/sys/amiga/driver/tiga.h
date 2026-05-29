
#define PRODUCT	0x04060000
#define HWLEN	0x00010000

#ifndef GSP_REG_DEFINED
#define GSP_REG_DEFINED
struct GSP_REG {
	unsigned short  hstadrl;
	unsigned short  hstadrh;
	unsigned short  hstdata;
	unsigned short  hstctl;
};
#endif

#define spltiga spl2
