/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  Quick and dirty strlen().
 *
 */

int strlen (char *s)
{
    int rtnval = 0;

    while (*s++ != '\000') {
	rtnval++;
    }
    return (rtnval);
}
