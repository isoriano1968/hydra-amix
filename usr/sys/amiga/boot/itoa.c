/*
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  Convert an integral value into a signed or unsigned ASCII
 *  form.  If the signedflag is true, then the value is treated
 *  as signed.  Otherwise it is treated as unsigned.
 * 
 *  The output is written to the destination string in reverse
 *  order, and then reversed again just prior to returning.
 *
 */

void itoa (char *string, int ival, int signedflag)
{
    unsigned int value;
    unsigned int temp1;
    unsigned int temp2;
    int savesign;
    char *stringbase;
    
    stringbase = string;
    if (signedflag & ((savesign = ival) < 0)) {
	value = -ival;
    } else {
	value = ival;
    }
    do {
	temp1 = value / 10;
	temp2 = temp1 * 10;
	*string++ = '0' + value - temp2;
    } while ((value = temp1) != 0);
    if (signedflag && (savesign < 0)) {
	*string++ = '-';
    }   
    *string = '\000';
    string--;
    while (stringbase < string) {
	temp1 = *stringbase;
	*stringbase++ = *string;
	*string-- = temp1;
    }
}
