/*
 *
 *  Copyright (C) 1991, Commodore Business Machines, Inc.
 *
 *  FILE
 *
 *	usermsg.c    support routines for user messages
 *
 *  DESCRIPTION
 *
 *	This module contains the first layer of interface routines
 *	to send informational messages to the user.
 *
 *	The following classes of messages, directed towards the user,
 *	are defined:
 *
 *	(1)	Error messages.   These messages are informational
 *		one way messages to the user, and require no direct
 *		response.  They are used to report errors, which
 *		may or may not be fatal.
 *
 *	(2)	Warning messages.   These messages are informational
 *		one way messages to the user, and require no direct
 *		response.  They are used to report information which
 *		is important for the user to know, but which is not
 *		serious enough to classify as an error.
 *
 *	(3)	Verbosity messages.  These messages are informational
 *		one way messages to the user, and require no direct
 *		response.  They are used to report information about
 *		the archive, current processing status, etc.
 *
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "usermsg.h"

/*
 *	Define structure for organizing information about each
 *	possible user message.  Each message has a unique message
 *	number.
 */

struct usermsg {
    int msg_no;			/* The internal message number */
    int msg_flags;		/* Some control flags */
    int msg_type;		/* Type of message (error, warning, etc) */
    char *msg_shortfmt;		/* Short form args format */
    char *msg_longfmt;		/* Format string for message */
};

#define TYPE_ERROR	1	/* An error message */
#define TYPE_WARNING	2	/* A warning message */
#define TYPE_VERBOSITY	3	/* A verbosity message (status, info, etc) */

#define MSG_ERRNO	(1<<0)	/* A system error message available */
#define MSG_ID		(1<<1)	/* Include id string */
#define MSG_FATAL	(1<<2)	/* Exit with error status after reporting */

static int errors;
static int warnings;
static int verbosities;

static struct usermsg messages[] = {
    { MSG_ELFHDR, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "",
	  "can't read ELF header" },
    { MSG_ELFMAGIC, MSG_ID | MSG_FATAL, TYPE_ERROR, "",
	  "bad ELF magic number, not an ELF file" },
    { MSG_MPHT, MSG_ID | MSG_FATAL, TYPE_ERROR, "",
	  "missing program header table" },
    { MSG_PHTSEEK, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "",
	  "can't seek to program header table" },
    { MSG_BADPHT, MSG_ID | MSG_FATAL, TYPE_ERROR, "",
	  "bad program header table structure" },
    { MSG_PHTREAD, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "",
	  "can't read next program header table entry" },
    { MSG_LDSKIP, MSG_ID, TYPE_WARNING, "%u",
	  "warning - loadable segment %u skipped" },
    { MSG_TOOBIG, MSG_ID | MSG_FATAL, TYPE_ERROR, "%u",
	  "not enough space in bootblocks (max %u bytes)" },
    { MSG_BOOTSEEK, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "",
	  "can't seek to boot segment in ELF file" },
    { MSG_BOOTREAD, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "",
	  "can't read boot segment from ELF file" },
    { MSG_BOOTWRITE, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "",
	  "can't write boot image" },
    { MSG_LAYOUT, MSG_ID, TYPE_VERBOSITY, "%s %u %u",
	  "%s segment starts at %#x, %#x (%u) bytes" },
    { MSG_PHTLOAD, MSG_ID | MSG_FATAL, TYPE_ERROR, "",
	  "boot segment in ELF file not marked loadable" },
    { MSG_MALLOC, MSG_ID | MSG_ERRNO | MSG_FATAL, TYPE_ERROR, "%u",
	  "can't malloc %u bytes of memory" },
    { MSG_BOOTSIZE, MSG_ID, TYPE_ERROR, "%u",
	  "illegal boot size %u rounded up to %u" },
    { 0, 0, 0, NULL,
	  NULL }
};

/*
 *  FUNCTION
 *
 *	usermsg    print appropriate message for given error
 *
 *  SYNOPSIS
 *
 *	void usermsg (int msgno, ...)
 *
 *  DESCRIPTION
 *
 *	Handles a user message uniquely specified by "msgno".
 *	Note that values for "msgno" are given in "usermsg.h".
 *
 *	Note that sys_errlist[] and sys_nerr are supplied by the
 *	runtime environment in most standard Unix systems.  Other
 *	implementations may require this module to be customized.
 *
 */

void usermsg (int msgno, ...)
{
    struct usermsg *msgp;
    char *sp;
    int msgtype;
    va_list args;
    char prtbuf[3 * 1024];
    extern char *whoami;
    extern int sys_nerr;
    extern char *sys_errlist[];
    
    va_start (args, msgno);
    sp = prtbuf;
    for (msgp = messages; msgp -> msg_no != 0 && msgp -> msg_no != msgno; msgp++) {;}
    msgtype = msgp -> msg_type;
    if ((msgp -> msg_no == 0) || (msgp -> msg_flags & MSG_ID)) {
	(void) sprintf (sp, "%s: ", whoami);
	sp += strlen (sp);
    }
    if (msgp -> msg_no != msgno) {
	(void) sprintf (sp, "warning - unknown internal error %d!", msgno);
	sp += strlen (sp);
	errors++;
    } else {
	if (msgtype == TYPE_ERROR) {
	    errors++;
	} else if (msgtype == TYPE_WARNING) {
	    warnings++;
	} else if (msgtype == TYPE_VERBOSITY) {
	    verbosities++;
	}
	(void) vsprintf (sp, msgp -> msg_longfmt, args);
	sp += strlen (sp);
	if ((errno > 0) && (msgp -> msg_flags & MSG_ERRNO)) {
	    if (errno > sys_nerr) {
		(void) sprintf (sp, ": !! bad errno %d !!", errno);
	    } else {
		(void) sprintf (sp, ": %s", sys_errlist[errno]);
	    }
	    sp += strlen (sp);
	}
    }
    (void) fprintf (stderr, "%s\n", prtbuf);
    fflush (stderr);
    if (msgp -> msg_flags & MSG_FATAL) {
	exit (1);
    }
    va_end (args);
}
