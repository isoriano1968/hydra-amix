/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *	Patchlevel: reltest.c_1.1
 *
 *	Simple little test driver program that opens each ELF file specified,
 *	calls reloc(), and then writes the resulting relocated text and data
 *	sections to a file who's name is derived from the input file name
 *	by appending ".rel".
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if DBUG
#    include <local/dbug.h>
#else
#    define DBUG_ENTER(a1)
#    define DBUG_RETURN(a1) return(a1)
#    define DBUG_VOID_RETURN return
#    define DBUG_EXECUTE(keyword,a1)
#    define DBUG_PRINT(keyword,arglist)
#    define DBUG_PUSH(a1)
#    define DBUG_POP()
#    define DBUG_PROCESS(a1)
#    define DBUG_FILE (stderr)
#    define DBUG_SETJMP setjmp
#    define DBUG_LONGJMP longjmp
#endif

#include <elf.h>
#include "bootdata.h"

main (argc, argv)
int argc;
char *argv[];
{
    int c;
    extern int optind;
    extern int getopt ();
    extern char *optarg;
    char *filename;
    char outfile[256];
    char errbuf[256];
    struct stat statb;
    int fd = -1;
    char *fdata = NULL;
    struct BootData bd;
    extern char *malloc ();

    DBUG_ENTER ("main");
    while ((c = getopt (argc, argv, "#:")) != EOF) {
	switch (c) {
	    case '#':
	        DBUG_PUSH (optarg);
		break;
	}
    }
    for ( ; optind < argc; optind++) {
	filename = argv[optind];
	if ((fd = open (filename, O_RDONLY, 777)) == -1) {
	    sprintf (errbuf, "can't open '%s' for read", filename);
	    perror (errbuf);
	} else if (stat (filename, &statb) == -1) {
	    sprintf (errbuf, "can't stat '%s'", filename);
	    perror (errbuf);
	} else if ((fdata = malloc (statb.st_size)) == NULL) {
	    sprintf (errbuf, "can't malloc %u bytes", statb.st_size);
	    perror (errbuf);
	} else if (read (fd, fdata, statb.st_size) != statb.st_size) {
	    sprintf (errbuf, "can't read '%s'", filename);
	    perror (errbuf);
	} else {
	    if (rel (fdata, 0x07800000, &bd)) {
		sprintf (outfile, "%s.rel", filename);
		close (fd);
		if ((fd = open (outfile, O_WRONLY | O_CREAT, 0777)) == -1) {
		    sprintf (errbuf, "can't open '%s' for write", outfile);
		    perror (errbuf);
		} else if (write (fd, fdata + bd.bd_toffset,
				  bd.bd_tsize) != bd.bd_tsize) {
		    sprintf (errbuf, "can't write '%s'", outfile);
		    perror (errbuf);
		} else if (write (fd, fdata + bd.bd_roffset,
				  bd.bd_rsize) != bd.bd_rsize) {
		    sprintf (errbuf, "can't write '%s'", outfile);
		    perror (errbuf);
		} else if (write (fd, fdata + bd.bd_doffset,
				  bd.bd_dsize) != bd.bd_dsize) {
		    sprintf (errbuf, "can't write '%s'", outfile);
		    perror (errbuf);
		}
	    }
	}
	if (fdata != NULL) {
	    free (fdata);
	    fdata = NULL;
	}
	if (fd >= 0) {
	    close (fd);
	}
    }
    DBUG_RETURN (0);
}
