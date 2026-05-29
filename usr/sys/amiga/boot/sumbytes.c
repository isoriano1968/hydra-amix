/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	Compute the unsigned sum of a specified length of memory,
 *	treating each byte as unsigned.
 */

unsigned long sumbytes (unsigned char *memory, unsigned long nbytes)
{
    unsigned long sum = 0;

    while (nbytes-- > 0) {
	sum += *memory++;
    }
    return (sum);
}

