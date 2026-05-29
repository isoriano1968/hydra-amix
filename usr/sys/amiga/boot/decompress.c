/*
 *  FILE
 *
 *	decompress.c    file decompression ala IEEE Computer, June 1984.
 *
 *  DESCRIPTION
 *
 *	Algorithm from "A Technique for High Performance Data Compression",
 *	Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 *	Algorithm:
 *
 * 	Modified Lempel-Ziv method (LZW).  Basically finds common
 *	substrings and replaces them with a variable size code.  This is
 *	deterministic, and can be done on the fly.  Thus, the decompression
 *	procedure needs no input table, but tracks the way the table was built.
 *
 *  NOTES
 *
 *	Based on: compress.c,v 4.0 85/07/30 12:50:00 joe Release $";
 *
 *	This code is derived from public domain code.  This version does
 *	decompression only, and has been highly modified for use in the CBM
 *	boot code environment.  The contributions of the original authors,
 *	noted below, are gratefully acknowledged.
 *
 *  AUTHORS
 *
 *	Spencer W. Thomas	decvax!harpo!utah-cs!utah-gr!thomas
 *	Jim McKie		decvax!mcvax!jim
 *	Steve Davies		decvax!vax135!petsd!peora!srd
 *	Ken Turkowski		decvax!decwrl!turtlevax!ken
 *	James A. Woods		decvax!ihnp4!ames!jaw
 *	Joe Orost		decvax!vax135!petsd!joe
 *
 */

#if (DBUG || TEST || NOTBOOT)
#  include <stdio.h>
#  include <stdarg.h>
#  define ALERT(x,y)
#  define reloc(a)		a
#  define ALLOC(nbytes)		trymalloc (nbytes)
#else
#  define ALERT(x,y)		Alert(x,y)
#  define ALLOC(nbytes)		alloc (nbytes, MEMF_FAST)
#  pragma pack(2)
#  include <exec/types.h>
#  include <exec/memory.h>
#  include <exec/alerts.h>
#  pragma pack(4)
#endif

#if DBUG
#  include <local/dbug.h>
#else
#  define DBUG_ENTER(a1)
#  define DBUG_RETURN(a1) return(a1)
#  define DBUG_VOID_RETURN return
#  define DBUG_EXECUTE(keyword,a1)
#  define DBUG_PRINT(keyword,arglist)
#  define DBUG_PUSH(a1)
#  define DBUG_POP()
#  define DBUG_PROCESS(a1)
#  define DBUG_FILE (stderr)
#  define DBUG_SETJMP setjmp
#  define DBUG_LONGJMP longjmp
#endif

#define min(a,b)	((a)<(b)?(a):(b))

/*
 * a code_int must be able to hold 2**maxbits values of type int, and also -1
 */

typedef int code_int;
typedef long int count_int;
typedef unsigned char char_type;
typedef int boolean;

#define TRUE		1
#define FALSE		0
#define NULL		0
#define EOF		(-1)

/* Defines for third byte of header */

#define BIT_MASK	0x1f
#define BLOCK_MASK	0x80

/*
 * Masks 0x40 and 0x20 are free.  I think 0x20 should mean that there is
 * a fourth header byte (for expansion).
 */

#define INIT_BITS 9		/* initial number of bits/code */

/* Define upper and lower limits on number of bits */

#define MINMAXBITS 12

#ifdef lint
#define MAXMAXBITS (16)		/* shut up lint */
#else
#define MAXMAXBITS (sizeof(int) > 2 ? 16 : 15)
#endif

#define MAXCODE(n_bits)	((1 << (n_bits)) - 1)

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**maxbits characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i)		codetabof(i)
#define htabof(i)		gbls->htab[i]
#define codetabof(i)		gbls->codetab[i]
#define tab_suffixof(i)		((char_type *)(gbls->htab))[i]
#define de_stack		((char_type *)&tab_suffixof(1<<gbls->maxbits))

/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */

#define FIRST	257		/* first free entry */
#define	CLEAR	256		/* table clear output code */

#define MAGIC0	0x1F
#define MAGIC1	0x9D

/*
 *	The following macros and variables allow us to do our own
 *	buffering of input and output streams, without involving
 *	stdio.  This gains us slightly better error handling.
 */

#if CRAMPED
#define IOSIZE (256)
#else
#define IOSIZE (1024)		/* Chunk size for I/O */
#endif

#define PUTC(ch)	*gbls->outbufp++=(ch);if(++gbls->outcnt==IOSIZE)flushout(gbls)
#define GETC()		(gbls->incnt-->0?(int)*gbls->inbufp++:fillinbuf(gbls))
#define FFLUSH()	flushout(gbls)

/*
 *	Because the boot code can have no static data, we must collect
 *	all the things that are normally static into an dynamic structure
 *	that can be allocated at run time and passed around via a single
 *	pointer.
 */

struct globals {
    int n_bits;			/* current number of bits/code */
    int maxbits;		/* user setable max number bits/code */
    code_int maxcode;		/* maximum code, given n_bits */
    code_int maxmaxcode;	/* should NEVER generate this code */
    count_int *htab;
    unsigned short *codetab;
    code_int free_ent;		/* first unused entry */
    int block_compress;
    int clear_flg;
    int resultflag;
    unsigned char inbuf[IOSIZE];
    unsigned char outbuf[IOSIZE];
    int incnt;
    int outcnt;
    unsigned char *inbufp;
    unsigned char *outbufp;
    int (*gblk)();
    int (*pblk)();
    char_type rmask[9];
    code_int hsize[17];
    int size;
    int offset;
    char_type buf[MAXMAXBITS];
    unsigned long inputsize;
};

static char failmsg[] = {"Decompression failed!"};

static boolean alloctables (struct globals *gbls);
static code_int getcode (struct globals *gbls);
static int fillinbuf (struct globals *gbls);
static void flushout (struct globals *gbls);

/*
 *	Emit a message for NON-BOOT versions.
 */

#if (DBUG || TEST || NOTBOOT)

static void message (char *fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	fflush (stderr);
	va_end (args);
}

static void *trymalloc (int nbytes)
{
    void *newmem;
    extern char *malloc ();

    if ((newmem = (void *) malloc (nbytes)) == NULL) {
	message ("%s\nCan't allocate %u bytes", failmsg, nbytes);
	exit (1);
    }
    return (newmem);
}

#endif

/*
 *	In order to reduce the amount of space allocated statically
 *	(helps us to squeeze bru into split I&D model on a 286), we
 *	allocate the htab and codetab arrays dynamically during
 *	initialization.  This is slightly better practice anyway...
 */

static boolean alloctables (struct globals *gbls)
{
    unsigned int codetabsize;
    unsigned int htabsize;
    boolean result = TRUE;

    DBUG_ENTER ("alloctables");
    DBUG_PRINT ("maxbits", ("maxbits = %d", gbls -> maxbits));
    gbls -> maxmaxcode = 1 << gbls -> maxbits;
    DBUG_PRINT ("maxmaxcode", ("maxmaxcode = %d", gbls -> maxmaxcode));
    codetabsize = gbls -> hsize[gbls -> maxbits] * sizeof (unsigned short);
    gbls -> codetab = (unsigned short *) ALLOC (codetabsize);
    htabsize = gbls -> hsize[gbls -> maxbits] * sizeof (count_int);
    gbls -> htab = (count_int *) ALLOC (htabsize);
    DBUG_RETURN (result);
}

/*
 * Decompress.
 *
 * This routine adapts to the codes, building the "string" table on-the-fly;
 * requiring no table to be stored in the compressed data.  The tables used
 * herein are shared with those of the compress() routine.  See the
 * definitions above.
 *
 * Use the number of LZW bits stored in the data header to override any
 * existing value for number of bits.
 *
 * The third parameter is the number of input bytes to process.  If
 * getblk is to be called until it decides the end of the data is
 * reached, then inputsize should be the largest unsigned long value.
 *
 */

int decompress (int (*getblk) (), int (*putblk) (), unsigned long inputsize)
{
    char_type *stackp;
    int finchar;
    code_int code;
    code_int oldcode;
    code_int incode;
    struct globals *gbls;
    extern int memset ();

    DBUG_ENTER ("decompress");
    gbls = (struct globals *) ALLOC (sizeof (struct globals));
    (void) memset (gbls, 0, sizeof (struct globals));
    gbls -> block_compress = BLOCK_MASK;
    gbls -> inbufp = gbls -> inbuf;
    gbls -> outbufp = gbls -> outbuf;
    gbls -> gblk = getblk;
    gbls -> pblk = putblk;
    gbls -> inputsize = inputsize;
    gbls -> resultflag = 1;
    gbls -> rmask[0] = 0x00;
    gbls -> rmask[1] = 0x01;
    gbls -> rmask[2] = 0x03;
    gbls -> rmask[3] = 0x07;
    gbls -> rmask[4] = 0x0f;
    gbls -> rmask[5] = 0x1f;
    gbls -> rmask[6] = 0x3f;
    gbls -> rmask[7] = 0x7f;
    gbls -> rmask[8] = 0xff;
    gbls -> hsize[12] = 5003;		/* 12 bits (80% occupancy) */
    gbls -> hsize[13] = 9001;		/* 13 bits (91% occupancy) */
    gbls -> hsize[14] = 18013;		/* 14 bits (91% occupancy) */
    gbls -> hsize[15] = 35023;		/* 15 bits (94% occupancy) */
    gbls -> hsize[16] = 69001;		/* 16 bits (95% occupancy) */
    DBUG_PRINT ("zresult", ("result = %d", gbls -> resultflag));
    /* Check the magic number */
    if ((GETC () != MAGIC0) || (GETC () != MAGIC1)) {
	gbls -> resultflag = 0;
	message (reloc ("%s\nBad magic number"), reloc (failmsg));
	DBUG_PRINT ("zresult", ("result = %d", gbls -> resultflag));
    } else {
	gbls -> maxbits = GETC ();	/* set number of bits from file */
	gbls -> block_compress = gbls -> maxbits & BLOCK_MASK;
	gbls -> maxbits &= BIT_MASK;
	DBUG_PRINT ("maxbits", ("got maxbits = %d from file", gbls -> maxbits));
	if (gbls -> maxbits < MINMAXBITS) gbls -> maxbits = MINMAXBITS;
	if (gbls -> maxbits > MAXMAXBITS) {
	    message (reloc ("%s\nBad number of bits (%u)"), reloc (failmsg),
		     gbls -> maxbits);
	    DBUG_RETURN (0);
	}
	if (!alloctables (gbls)) {
	    DBUG_RETURN (0);
	}
	/* 
	 * Initialize the first 256 entries in the table.
	 */

	gbls -> maxcode = MAXCODE (gbls -> n_bits = INIT_BITS);
	for (code = 255; code >= 0; code--) {
	    tab_prefixof (code) = 0;
	    tab_suffixof (code) = (char_type) code;
	}
	gbls -> free_ent = ((gbls -> block_compress) ? FIRST : 256);
	
	finchar = oldcode = getcode (gbls);
	if (oldcode == -1) {	/* EOF already? */
	    DBUG_RETURN (0);			/* Get out of here */
	}
	PUTC ((char) finchar);
	/* first code must be 8 bits = char */
	stackp = de_stack;
	
	while ((code = getcode (gbls)) > -1) {
	    DBUG_PRINT ("code", ("next code = %#x", code));
	    if ((code == CLEAR) && gbls -> block_compress) {
		for (code = 255; code >= 0; code--) {
		    tab_prefixof (code) = 0;
		}
		gbls -> clear_flg = 1;
		gbls -> free_ent = FIRST - 1;
		if ((code = getcode (gbls)) == -1) {
		    DBUG_PRINT ("code", ("next code = %#x", code));
		    /* O, untimely death! */
		    DBUG_PRINT ("code", ("terminate from getcode gets EOF"));
		    break;
		}
	    }
	    incode = code;
	    /* 
	     * Special case for KwKwK string.
	     */
	    if (code >= gbls -> free_ent) {
		*stackp++ = (char_type) finchar;
		code = oldcode;
	    }
	    /* 
	     * Generate output characters in reverse order
	     */
	    while (code >= 256) {
		*stackp++ = tab_suffixof (code);
		code = tab_prefixof (code);
	    }
	    *stackp++ = finchar = tab_suffixof (code);
	    /* 
	     * And put them out in forward order
	     */
	    do {
		PUTC ((char) *--stackp);
		DBUG_PRINT ("code", ("*stackp = %#x", *stackp));
	    } while (stackp > de_stack);
	    /* 
	     * Generate the new entry.
	     */
	    if ((code = gbls -> free_ent) < gbls -> maxmaxcode) {
		tab_prefixof (code) = (unsigned short) oldcode;
		tab_suffixof (code) = (char_type) finchar;
		gbls -> free_ent = code + 1;
	    }
	    /* 
	     * Remember previous code.
	     */
	    oldcode = incode;
	}
	FFLUSH ();
    }
    DBUG_RETURN (gbls -> resultflag);
}


/*
 * TAG( getcode )
 *
 * Read one code from the standard input.  If EOF, return -1.
 * Inputs:
 * 	function pointer for output
 * Outputs:
 * 	code or -1 is returned.
 */

static code_int getcode (struct globals *gbls)
{
    code_int code;
    int r_off;
    int bits;
    char_type *bp;
    int nextch;
    
    DBUG_ENTER ("getcode");
    bp = gbls -> buf;
    if (gbls -> clear_flg > 0 || gbls -> offset >= gbls -> size || gbls -> free_ent > gbls -> maxcode) {
	/* 
	 * If the next entry will be too big for the current code
	 * size, then we must increase the size.  This implies reading
	 * a new buffer full, too.
	 */
	if (gbls -> free_ent > gbls -> maxcode) {
	    gbls -> n_bits++;
	    if (gbls -> n_bits == gbls -> maxbits) {
		gbls -> maxcode = gbls -> maxmaxcode;	    /* won't get any bigger now */
	    } else {
		gbls -> maxcode = MAXCODE (gbls -> n_bits);
	    }
	}
	if (gbls -> clear_flg > 0) {
	    gbls -> maxcode = MAXCODE (gbls -> n_bits = INIT_BITS);
	    gbls -> clear_flg = 0;
	}
	bp = gbls -> buf;
	for (gbls -> size = 0; gbls -> size < gbls -> n_bits; gbls -> size++) {
	    if ((nextch = GETC ()) == EOF) {
		break;
	    } else {
		DBUG_PRINT ("code", ("next byte from input = %#x", nextch));
		*bp++ = (char_type) nextch;
	    }
	}
	if (gbls -> size == 0) {
	    DBUG_PRINT ("code", ("return code %#x", -1));
	    DBUG_RETURN (-1);
	}
	bp = gbls -> buf;
	gbls -> offset = 0;
	/* Round size down to integral number of codes */
	gbls -> size = (gbls -> size << 3) - (gbls -> n_bits - 1);
    }
    r_off = gbls -> offset;
    bits = gbls -> n_bits;
    /* 
     * Get to the first byte.
     */
    bp += (r_off >> 3);
    r_off &= 7;
    /* Get first part (low order bits) */
#ifdef NO_UCHAR
    code = ((*bp++ >> r_off) & rmask[8 - r_off]) & 0xff;
#else
    code = (*bp++ >> r_off);
#endif	/* NO_UCHAR */
    bits -= (8 - r_off);
    r_off = 8 - r_off;		/* now, offset into code word */
    /* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
    if (bits >= 8) {
#ifdef NO_UCHAR
	code |= (*bp++ & 0xff) << r_off;
#else
	code |= *bp++ << r_off;
#endif /* NO_UCHAR */
	r_off += 8;
	bits -= 8;
    }
    /* high order bits. */
    code |= (*bp & gbls -> rmask[bits]) << r_off;
    gbls -> offset += gbls -> n_bits;
    DBUG_PRINT ("code", ("return code %#x", code));
    DBUG_RETURN (code);
}


static int fillinbuf (struct globals *gbls)
{
    int firstchar;
    int nbytes;

    DBUG_ENTER ("fillinbuf");
    gbls -> inbufp = gbls -> inbuf;
    nbytes = min (gbls -> inputsize, IOSIZE);
    if (nbytes == 0) {
	DBUG_PRINT ("inbuf", ("assume EOF"));
	firstchar = EOF;
	gbls -> incnt = 0;
    } else if ((gbls -> incnt = (*gbls -> gblk) ((char *) gbls -> inbuf, nbytes)) > 0) {
	DBUG_PRINT ("inbuf", ("read %d bytes from input", gbls -> incnt));
	firstchar = *gbls -> inbufp++;
	gbls -> inputsize -= nbytes;
	gbls -> incnt--;
    } else if (gbls -> incnt == -1) {
	gbls -> resultflag = 0;
	message ("%s\nCan't read %u bytes", reloc (failmsg), IOSIZE);
	DBUG_PRINT ("zresult", ("result = %d", gbls -> resultflag));
	firstchar = EOF;
	gbls -> incnt = 0;
    } else {
	DBUG_PRINT ("inbuf", ("found EOF"));
	firstchar = EOF;
	gbls -> incnt = 0;
    }
    DBUG_RETURN (firstchar);
}

/*
 *	Flush output buffer.
 *
 *	Note that we only allow seeking during decompression, and only
 *	when the buffer we are about to write is NOT the last buffer
 *	for the file.  Otherwise, we would do the seek (which doesn't
 *	actually extend the file) and then close the file, leaving
 *	it truncated to the size it had at the last write of non-null
 *	data.  In other words, if the file ended in a bunch of nulls,
 *	all of the trailing nulls would be truncated if we didn't
 *	actually write the last buffer with "real" nulls.
 *
 */

static void flushout (struct globals *gbls)
{
    int bytesout;

    DBUG_ENTER ("flushout");
    DBUG_PRINT ("outbuf", ("flush %d bytes from outbuf", gbls -> outcnt));
    if (gbls -> outcnt > 0) {
	bytesout = (*gbls -> pblk) ((char *) gbls -> outbuf, (unsigned int) gbls -> outcnt);
	if (bytesout != gbls -> outcnt) {
	    message (reloc ("%s\nCan't write %u bytes"), reloc (failmsg),
		     gbls -> outcnt);
	    gbls -> resultflag = 0;
	    DBUG_PRINT ("zresult", ("result = %d", gbls -> resultflag));
	}
    }
    gbls -> outcnt = 0;
    gbls -> outbufp = gbls -> outbuf;
    DBUG_VOID_RETURN;
}

#ifdef TEST

/*
 *	A simple test driver.  Reads from stdin and writes to stdout.
 */

static int readblock (char *buf, int nbytes)
{
    extern int read ();

    DBUG_ENTER ("readblock");
    nbytes = read (0, buf, (unsigned int) nbytes);
    DBUG_RETURN (nbytes);
}

static int writeblock (char *buf, int nbytes)
{
    extern int write ();

    DBUG_ENTER ("writeblock");
    nbytes = write (1, buf, (unsigned int) nbytes);
    DBUG_RETURN (nbytes);
}

main (argc, argv)
int argc;
char *argv[];
{
    int c;
    extern int getopt ();
    extern char *optarg;
    extern void exit ();

    while ((c = getopt (argc, argv, "#:")) != EOF) {
	switch (c) {
	    case '#':
		DBUG_PUSH (optarg);
		break;
	}
    }
    if (!decompress (readblock, writeblock, ~0L)) {
	(void) fprintf (stderr, "Decompression failed.\n");
    }
    exit (0);
    return (0);		/* shut up lint */
}

#endif

