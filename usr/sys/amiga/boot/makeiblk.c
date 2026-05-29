/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	Make a boot information block.  The boot information block
 *	preceeds each file in the boot image, and contains information
 *	used to read and validate the file, such as the file size and
 *	checksum.
 */
 
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/elf.h>

#include "bootdata.h"
#include "infoblock.h"

#define ZMAGIC	0x1F9D				/* LZW magic number */

static Elf32_Addr bindaddr = 0xFFFFFFFF;	/* default is auto binding */
static struct infoblock iblock;
static int fullsize = 0;
static int infd = -1;

/*
 *	Produces a checksum of a byte stream that should be the same as
 *	the standard SVR4 "sum" program.  Note that the "sum" documentation
 *	is misleading, the checksum is NOT simply a 16-bit checksum of all
 *	the bytes.  Here is the algorithm:
 *
 *	1.	Add up all the bytes using an unsigned long (32-bit)
 *		integer.
 *
 *	2.	Add the overflow from the lower 16 bits (the high 16
 *		bits) back into the lower 16 bits using unsigned
 *		arithmetic.  I.E. treat the 32-bit long as two 16-bit
 *		unsigned ints and add them using 32-bit unsigned
 *		arithmetic.
 *
 *	3.	Repeat step (2) one more time, presumably to catch
 *		any additional overflow.
 *
 */

#define BFSIZ 1024

unsigned long chksum (int fd, char *kfile, char *argv0)
{
    unsigned char buf[BFSIZ];
    int nbytes;
    unsigned long sum = 0;
    unsigned long temp = 0;

    while ((nbytes = read (fd, buf, sizeof (buf))) > 0) {
	while (nbytes-- > 0) {
	    sum += buf[nbytes];
	}
    }
    if (nbytes == -1) {
	fprintf (stderr, "%s: can't read '%s': ", argv0, kfile);
	perror ("");
	exit (1);
    }
    temp = (sum >> 16) + (sum & 0xFFFF);
    sum = (temp >> 16) + (temp & 0xFFFF);
    return (sum);
}

static int readblock (char *buf, int nbytes)
{
    extern int read ();

    nbytes = read (infd, buf, (unsigned int) nbytes);
    return (nbytes);
}

static int countbytes (char *buf, int nbytes)
{
    fullsize += nbytes;
}

main (argc, argv)
int argc;
char *argv[];
{
    int c;
    int outfd = 1;
    struct stat sbuf;
    unsigned short zmagic;

    if (argc < 2 || argc > 3) {
	fprintf (stderr, "usage: %s infile [outfile]\n", argv[0]);
	exit (1);
    } else if (stat (argv[1], &sbuf) == -1) {
	fprintf (stderr, "%s: can't stat '%s': ", argv[0], argv[1]);
	perror ("");
	exit (1);
    } else if ((infd = open (argv[1], O_RDONLY, 0666)) == -1) {
	fprintf (stderr, "%s: can't open '%s' for read: ", argv[0], argv[1]);
	perror ("");
	exit (1);
    } else if (argc > 2 &&
	       (outfd = open (argv[2], O_WRONLY | O_CREAT, 0666)) == -1) {
	fprintf (stderr, "%s: can't open '%s' for write: ", argv[0], argv[2]);
	perror ("");
	exit (1);
    } else {
	read (infd, &zmagic, sizeof (zmagic));
	lseek (infd, 0L, 0);
	strcpy (iblock.ib_ident, "IBLK");
	iblock.ib_size = sbuf.st_size;
	if (zmagic == ZMAGIC) {
	    decompress (readblock, countbytes, ~0L);
	    iblock.ib_fullsize = fullsize;
	    lseek (infd, 0L, 0);
	}
	iblock.ib_chksum = chksum (infd, argv[1], argv[0]);
	iblock.ib_bind = bindaddr;
	if (write (outfd, &iblock, sizeof (iblock)) != sizeof (iblock)) {
	    fprintf (stderr, "%s: can't write '%s': ", argv[0], argv[2]);
	    perror ("");
	    exit (1);
	}
    }
    if (outfd >= 0) {
	close (outfd);
    }
    if (infd >= 0) {
	close (infd);
    }
    exit (0);
}

