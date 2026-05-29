/*
 * The following definitions make C more amenable to a purist.
 */
#define	bool		char		/* boolean type */
#define	uint		unsigned int	/* short names for unsigned types */
#define	ulong		unsigned long
#define	uchar		unsigned char
#define	ushort		unsigned short int
#define	not		!		/* logical negation operator */
#define	and		&&		/* logical conjunction */
#define	or		||		/* logical disjunction */
#define	TRUE		(0 == 0)
#define	FALSE		(not TRUE)
#define	loop		while (TRUE)	/* loop until break */
#define	unless(p)	if (! (p))
#define	until(p)	while (! (p))
#define	EOS	'\0'			/* end-of-string char */
#define	NULL	0			/* invalid pointer */

#define	cardof(a)	(sizeof(a) / sizeof(*(a)))
#define	endof(a)	((a) + cardof(a))
#define	bitsof(a)	(sizeof(a) * 8)
#define	maskover(n)	(((uint)1 << (n)) - 1)


/*
 * Assertion verifier.
 */
#ifndef	NODEBUG				/* asserts checked */
#define	assert(p)							\
		if (! (p)) {						\
			printf("%s (%d): %s\n", __FILE__, __LINE__, #p);\
			panic("Assert failure");			\
		} else
#else					/* asserts unchecked */
#define	assert(p)
#endif
