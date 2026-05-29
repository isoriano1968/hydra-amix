/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	Produces a checksum of a byte stream that should be the same as
 *	the standard SVR4 "sum" program.  Note that the "sum" documentation
 *	is misleading, the checksum is NOT simply a 16-bit checksum of all
 *	the bytes.  Here is the algorithm:
 *
 *	1.	Add up all the bytes using an unsigned long (32-bit)
 *		integer, treating each byte as unsigned.
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

unsigned long chksum (unsigned char *memory, unsigned long nbytes)
{
    unsigned long sum = 0;
    unsigned long temp;

    while (nbytes-- > 0) {
	sum += *memory++;
    }
    temp = (sum >> 16) + (sum & 0xFFFF);
    sum = (temp >> 16) + (temp & 0xFFFF);
    return (sum);
}

#if TESTCHKSUM	/* Read stdin up to 2Mb, compute chksum, and print it */

unsigned char buf[2 * 1024 * 1024];

main ()
{
    unsigned long nbytes;

    nbytes = read (0, buf, sizeof (buf));
    printf ("%u\n", chksum (buf, nbytes));
}

#endif
