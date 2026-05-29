/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  Compare first N bytes of two strings, returning 0 if they are not
 *  equal and non-zero if they are.
 */

int streqn (char *s1, char *s2, int nbytes)
{
    while ((*s1 != '\000') && (*s1 == *s2) && (--nbytes > 0)) {
	s1++;
        s2++;
    }
    return (*s1 == *s2);
}
