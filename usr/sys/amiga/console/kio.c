#include "amigahr.h"


#define BAUD 9600


void KPutChar(c)
unsigned char c;
{
    AMIGA->serper = (3579098+BAUD/2)/BAUD - 1;
    if (c=='\n')
    {
	while ((AMIGA->serdatr&0x2000) == 0)
	    ;
	AMIGA->serdat = 0x100 | '\r';
    }

    while ((AMIGA->serdatr&0x2000) == 0)
	;
    AMIGA->serdat = 0x100 | c;
}


unsigned char KGetChar()
{
    unsigned short x = AMIGA->intenar & AIESRBF;

    AMIGA->serper = (3579098+BAUD/2)/BAUD - 1;
    AMIGA->intena = (AINTCLR|AIESRBF);
    while (!(AMIGA->intreqr&AIESRBF))
	;

    AMIGA->intreq = (AINTCLR|AIESRBF);
    if (x)
	AMIGA->intena = (AINTSET|x);

    return AMIGA->serdatr & 0xFF;
}
