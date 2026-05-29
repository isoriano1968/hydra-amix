/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  Compare two strings, returning 0 if they are not equal and
 *  non-zero if they are.
 *
 */

int streq (char *s1, char *s2)
{
    while ((*s1 != '\000') && (*s1 == *s2)) {
	s1++;
        s2++;
    }
    return (*s1 == *s2);
}
