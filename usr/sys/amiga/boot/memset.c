/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  Brute force memset, one byte at a time.
 *
 */

char *memset (char *s, int c, int n)
{
    char *copy = s;

    while (n-- > 0) {
	*copy++ = c;
    }
    return (s);
}
