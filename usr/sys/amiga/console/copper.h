
/*
 * Word extraction.
 */
#define	loww(x)		((unsigned short)((unsigned long)(x)))
#define highw(x)	((unsigned short)((unsigned long)(x) >> 16))


/*
 * Copper instructions.
 * These are macros which store the specified instruction to "*copp++".
 * "copp" should be an (unsigned short *).
 *
 * The first argument to move is the name of an AMIGA register.
 * The hpos argument to wait is in lo-res pixels.
 */

#define	CAMIGA			((struct amiga *)0)
#define	amigaoffset(reg)	loww(&CAMIGA->reg)

#define	move(reg, val)		(*copp++ = amigaoffset(reg)),		\
				(*copp++ = loww(val))

#define	movel(reg, val)		(*copp++ = amigaoffset(reg)),		\
				(*copp++ = highw(val)),			\
				(*copp++ = amigaoffset(reg)+2),	\
				(*copp++ = loww(val))

#define	waitm(vpos, hpos, vmsk, hmsk) \
				(*copp++ = (vpos)<<8 | (hpos)/2 | 1), \
				(*copp++ = 1<<15 | (vmsk)<<8 | (hmsk)<<1)

#define	wait(vpos, hpos)	waitm(vpos, hpos, 0x7f, 0x7f)

#define	skipm(vpos, hpos, vmsk, hmsk) \
				(*copp++ = (vpos)<<8 | (hpos)/2 | 1), \
				(*copp++ = 1<<15 | (vmsk)<<8 | (hmsk)<<1 | 1)

#define	skip(vpos, hpos)	skipm(vpos, hpos, 0x7f, 0x7f);

#define	stop()			wait(0xFF, 0x1FF)



#ifdef DUMP_COPLIST
static void dump_coplist(copp)
register unsigned long *copp;
{
	register unsigned long ins;

	do {
		ins = *copp++;
		printf("%x: %x\t", (copp-1), ins);
		if (!(ins&0x10000))
			printf("move(0x%x, 0x%x)\n",
				highw(ins), loww(ins));
		else
			printf("%s(0x%x, 0x%x)\n",
				((ins&0x0001) ? "skip" : "wait"),
				(ins>>24)&0x7f, (ins>>16)&0xfe);
	} while (ins != 0xfffffffe &&
		 highw(ins) != 0x0088 &&
		 highw(ins) != 0x008a);
}
#endif
