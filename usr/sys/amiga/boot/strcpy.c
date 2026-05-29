/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  Quick and dirty strcpy().
 *
 */

char *strcpy (char *s1, char *s2)
{
    char *rtnval = s1;

    while ((*s1++ = *s2++) != '\000') {;}
    return (rtnval);
}
