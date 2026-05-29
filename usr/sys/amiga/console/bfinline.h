#ifdef __GNUC__

static inline unsigned int bfffo(void *buf,
				 unsigned int bitoff, unsigned int nbits)
{
 unsigned int result;
 asm("bfffo (%1){%2:%3},%0" :
     "=d"(result) : "a"(buf), "d"(bitoff), "d"(nbits));
 return result;
}

static inline void bfchg(void *buf, unsigned int bitoff, unsigned int nbits)
{asm("bfchg (%0){%1:%2}" : : "a"(buf), "d"(bitoff), "d"(nbits));}

static inline void bfclr(void *buf, unsigned int bitoff, unsigned int nbits)
{asm("bfclr (%0){%1:%2}" : : "a"(buf), "d"(bitoff), "d"(nbits));}

static inline void bfset(void *buf, unsigned int bitoff, unsigned int nbits)
{asm("bfset (%0){%1:%2}" : : "a"(buf), "d"(bitoff), "d"(nbits));}

static inline int bfexts(void *buf, unsigned int bitoff, unsigned int nbits)
{
 int result;
 asm("bfexts (%1){%2:%3},%0" :
     "=d"(result) : "a"(buf), "d"(bitoff), "d"(nbits));
 return result;
}

static inline unsigned int bfextu(void *buf,
				  unsigned int bitoff, unsigned int nbits)
{
 unsigned int result;
 asm("bfextu (%1){%2:%3},%0" :
     "=d"(result) : "a"(buf), "d"(bitoff), "d"(nbits));
 return result;
}

static inline void bfins(void *buf, unsigned int bitoff, unsigned int nbits,
			 unsigned int val)
{asm("bfins %0,(%1){%2:%3}" : : "d"(val), "a"(buf), "d"(bitoff), "d"(nbits));}

#else

asm unsigned int bfffo(buf, bitoff, nbits)
{
%	mem	buf, bitoff, nbits;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	long	0xEDD00821
}
/* 0xEDD00821 is: bfffo (%a0){%d0,%d1},%d0 */

asm void bfchg(buf, bitoff, nbits)
{
%	mem	buf, bitoff, nbits;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	long	0xEAD00821
}
/* 0xEAD00821 is: bfchg (%a0){%d0,%d1} */

asm void bfclr(buf, bitoff, nbits)
{
%	mem	buf, bitoff, nbits;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	long	0xECD00821
}
/* 0xECD00821 is: bfclr (%a0){%d0,%d1} */

asm void bfset(buf, bitoff, nbits)
{
%	mem	buf, bitoff, nbits;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	long	0xEED00821
}
/* 0xEED00821 is: bfset (%a0){%d0,%d1} */

asm long bfexts(buf, bitoff, nbits)
{
%	mem	buf, bitoff, nbits;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	long	0xEBD00821
}
/* 0xEBD00821 is: bfexts (%a0){%d0,%d1},%d0 */

asm unsigned long bfextu(buf, bitoff, nbits)
{
%	mem	buf, bitoff, nbits;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	long	0xE9D00821
}
/* 0xE9D00821 is: bfextu (%a0){%d0,%d1},%d0 */

asm void bfins(buf, bitoff, nbits, val)
{
%	mem	buf, bitoff, nbits, val;

	mov.l	buf,%a0
	mov.l	bitoff,%d0
	mov.l	nbits,%d1
	mov.l	%d2,%a1
	mov.l	val,%d2
	long	0xEFD02821
	mov.l	%a1,%d2
}
/* 0xEFD02821 is: bfins %d2,(%a0){%d0,%d1} */

#endif /* GNUC */
