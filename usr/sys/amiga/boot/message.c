/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *	Display a message for the user, using a "printf" like format for
 *	the message specification, and using the DisplayAlert() function
 *	to post the message.
 *
 *	The message string is built in a dynamically allocated buffer
 *	of fixed length (typically 1Kb), which should be plenty for almost
 *	any conceivable message.
 *
 *	Format specifications include:
 *
 *		%s	Copy the next argument as a string.
 *		%x	Format next arg as a hexadecimal value.
 *		%d	Format next arg as a signed decimal value.
 *		%u	Format next arg as an unsigned decimal value.
 *		\n	Terminate message line and start another.
 *		%B	Next arg is number of blanks to insert.
 *		%M	Insert MOUSE_MESSAGE (see below).
 *		%%	A literal % character in output.
 */

#include <stdarg.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>

#if amigados
#include <proto/exec.h>
#else
extern void *AllocMem (int nbytes, int flags);
#endif

#define DISPBUFSIZE	1024
#define MOUSE_MESSAGE	"Press a mouse button to continue."


static void BuildString (char *bufp, int *heightp, char *fmt, va_list ap)
{
	int ival;
	int xpos;
	int ypos;
	int nbli;
	unsigned int nbl;
	char *str;

	xpos = 20;
	ypos = 16;
	*bufp++ = xpos >> 8;
	*bufp++ = xpos & 0xFF;
	*bufp++ = ypos;
	do {
	    if (*fmt == '\n') {
		/* found end of line, start a new one */
		*bufp++ = '\000';
		*bufp++ = '\001';
		*bufp++ = xpos >> 8;
		*bufp++ = xpos & 0xFF;
		ypos += 14;
		*bufp++ = ypos;
		fmt++;
	    } else if (*fmt == '%') {
		/* start of a format spec */
		switch (*++fmt) {
		    case 's':
		        str = va_arg (ap, char *);
			while ((*bufp++ = *str++) != '\000') {;}
			bufp--;
			break;
		    case 'x':
			*bufp++ = '0';
			*bufp++ = 'x';
			ival = va_arg (ap, int);
			for (nbli = 7; nbli >= 0; nbli--) {
				nbl = (ival >> (nbli * 4)) & 0xF;
				if (nbl > 0x9) {
					*bufp++ = 'A' + nbl - 0xA;
				} else {
					*bufp++ = '0' + nbl - 0x0;
				}	    
			}
			break;
		    case 'd':
			ival = va_arg (ap, int);
			itoa (bufp, ival, 1);
			while (*bufp != '\000') {
			    bufp++;
			}
			break;
		    case 'u':
			ival = va_arg (ap, int);
			itoa (bufp, ival, 0);
			bufp += strlen (bufp);
			break;
		    case 'B':
			ival = va_arg (ap, int);
			while (ival-- > 0) {
				*bufp++ = ' ';
			}
			break;
		    case 'M':
			(void) strcpy (bufp, reloc (MOUSE_MESSAGE));
			bufp += strlen (bufp);
			break;
		    case '%':
			*bufp++ = *fmt;
			break;
		}
		fmt++;
	    } else {
		/* normal character, just copy to buffer */
		*bufp++ = *fmt++;
	    }
	} while (*fmt != '\000');
	*bufp++ = '\000';
	*heightp = ypos + 12;
}

int message (char *fmt, ...)
{
	int response = 0;
	int height;
	char *dispbuf;
	va_list args;

	va_start (args, fmt);
	dispbuf = AllocMem (DISPBUFSIZE, MEMF_PUBLIC | MEMF_CLEAR);
	BuildString (dispbuf, &height, fmt, args);
	response = DisplayAlert (RECOVERY_ALERT, dispbuf, height);
	FreeMem (dispbuf, DISPBUFSIZE);
	va_end (args);
	return (response);
}

#if TEST

struct IntuitionBase *IntuitionBase;

main ()
{
	int response;

	IntuitionBase = (struct IntuitionBase *) 
			OpenLibrary ("intuition.library", 0);

	response = message ("Line 1\nLine 2\nLine 3");
	printf ("response was %d\n", response);

	response = message ("Test value %x %% %x", 0x12345678, 0x23456789);
	printf ("response was %d\n", response);

	response = message ("0 blanks ->%B<-\n1 blank  ->%B<-\n3 blanks ->%B<-",
				0, 1, 3);
	printf ("response was %d\n", response);
}

#endif
