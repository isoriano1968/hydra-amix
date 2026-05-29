/* Copyright (C) 1991, Commodore Business Machines, Inc. */

/*
 *	Blink the power led the specified number of times.
 *
 *	The BUSYWAIT define may need tweaking depending upon how fast
 *	the processor is.  (this is just a debugging hack anyway...)
 */

#pragma pack(2)
#include <exec/types.h>
#include <hardware/cia.h>
#pragma pack(4)

#define ACIAA ((struct CIA *)0xbfe001)
#define BUSYWAIT 0x40000

void BlinkLED (n)
int n;
{
    int i;

    while (n--) {
	ACIAA -> ciapra &= ~CIAF_LED;
	for (i = 0; i < BUSYWAIT; ++i) {;}
	ACIAA -> ciapra |= CIAF_LED;
	for (i = 0; i < BUSYWAIT; ++i) {;}
    }
}
