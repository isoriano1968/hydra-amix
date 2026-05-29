/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *      Patchlevel: elf2brel.c_1.1
 *
 *
 *	Converts a standard relocatable ELF file into a special custom
 *	relocatable ELF file, that the UNIX boot loader recognizes.
 *	This special form is much more compact than the original ELF
 *	file, since it no longer has a symbol table or string table,
 *	which accounts for about 35% of the file space in a typical
 *	relocatable kernel.  This helps to squeeze kernels onto a
 *	single boot floppy.
 *
 *	The compact form is still a well formed ELF file, it is just
 *	no longer usable as a relocatable file by the normal linker.
 *	For this form, the .text, .rodata, .data, and .bss sections are
 *	combined into a single contiguous section, the relocation tables
 *	for the .text, .rodata, and .data sections are combined into a
 *	single more compact relocation table, and all other parts of the
 *	file not useful to the boot loader are discarded (such as the
 *	symbol table and string table).
 *
 *	The modified relocation table entries contain only two values,
 *	the offset of the relocation slot from the start of the single
 *	.text section, and a relocation value (relative to the start of
 *	the single .text section).  The most significant bit of the
 *	relocation slot index is hijacked to flag whether or not the
 *	relocation value is an absolute value or a value relative to the
 *	start of the .text section.  The section types of these modified
 *	relocation tables are changed in the ELF section header to be
 *	application specific, as allowed by the ELF standard, so that
 *	standard tools will ignore them.  This more compact form of
 *	the relocation tables typically reduce the file size another
 *	5% to 10%.
 *
 *	At boot time, all the boot loader must do is pick a binding
 *	address for the start of the .text section, and then process
 *	the relocation table, computing the final relocatable value
 *	using the binding address and stuffing it into the relocation
 *	slot indexed by the offset field.
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <elf.h>
#include <sys/elf_68K.h>

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

char *whoami = "urk!";

static Elf32_Ehdr *Ehdr;		/* Pointer to the ELF header */
static Elf32_Shdr *Shdr;		/* Pointer to section header table */
static char *Elfdata;			/* In memory copy of ELF file */
static int vflag;			/* -v flag (verbose) */

static unsigned long textsize;		/* Size of text section */
static unsigned long rodatasize;	/* Size of rodata section */
static unsigned long datasize;		/* Size of data section */
static unsigned long bsssize;		/* Size of bss section */
static unsigned long nrelcount;		/* Number of new reloc entries */

static struct nrel {
    unsigned long nr_offset;
    unsigned long nr_value;
} *nreltab;				/* New relocation table */


static struct {				/* Indices of important sections */
    int i_text;
    int i_rodata;
    int i_data;
    int i_bss;
    int i_symtab;
    int i_strtab;
    int i_shstrtab;
    int i_rela_text;
    int i_rela_data;
    int i_rela_rodata;
} i;

/*
 *	Forward declarations of local functions.
 */

static void Options (int argc, char *argv[]);
static void ReadElfFile ();
static void ProcessSymtab ();
static void GenRelocTable ();
static void WriteElfFile ();

/*
 *	The basic strategy is as follows:
 *
 *	(1)	Read the entire ELF file into a contiguous section of
 *		memory and set up pointers to the portions that we are
 *		interested in.
 *
 *	(2)	Process each symbol table entry, modifying the .text,
 *		.data, or .bss symbols to be relative to the start
 *		of a combined, contiguous section, and allocating
 *		COMMON symbols at the end.  Also set the value of
 *		some special symbols like "etext", "edata", and "end".
 *
 *	(3)	Generate a new combined relocation table using the
 *		modified symbol table.
 *
 *	(4)	Write out the compacted relocatable ELF file.
 *
 */

main (argc, argv)
int argc;
char *argv[];
{
    int c;
    extern int optind;
    extern int getopt ();
    extern char *optarg;

    DBUG_ENTER ("main");
    Options (argc, argv);
    ReadElfFile ();
    ProcessSymtab ();
    GenRelocTable ();
    WriteElfFile ();
    DBUG_RETURN (0);
}

/*
 *	Process command line options using the standard getopt().
 */

static void Options (int argc, char *argv[])
{
    int c;
    extern char *optarg;
    extern int getopt ();

    DBUG_ENTER ("Options");
    whoami = argv[0];
    while ((c = getopt (argc, argv, "v#:")) != EOF) {
	switch (c) {
	    case '#':
		DBUG_PUSH (optarg);
		DBUG_PRINT ("dbug", ("dbug options '%s'", optarg));
		break;
	    case 'v':
		vflag++;
		break;
	}
    }
    DBUG_VOID_RETURN;
}

/*
 *	Find out how big the input file on stdin is, allocate a buffer
 *	for it, read the whole thing in one fell swoop, and verify from
 *	the magic number that it is an ELF file, verify from the type
 *	that it is a relocatable file, and pick it apart.
 *
 *	Note:  We don't try to be fully general or fancy here in finding
 *	the sections we are interested in.  We just us the brute force
 *	method of check the section name and matching against various
 *	ones we are interested in, assuming that there is one and only
 *	one that matches.  If any are missing, it is a fatal error.
 */

static void ReadElfFile ()
{
    struct stat statb;
    char errbuf[256];
    Elf32_Shdr *shdrp;
    unsigned int index;
    char *shstrtab;
    char *shdrname;
    extern char *malloc ();

    DBUG_ENTER ("ReadElfFile");
    if (fstat (0, &statb) == -1) {
	sprintf (errbuf, "%s: can't stat stdin", whoami);
	perror (errbuf);
	exit (1);
    } else if ((Elfdata = malloc (statb.st_size)) == NULL) {
	sprintf (errbuf, "%s: can't malloc %u bytes", whoami, statb.st_size);
	perror (errbuf);
	exit (1);
    } else if (read (0, Elfdata, statb.st_size) != statb.st_size) {
	sprintf (errbuf, "%s: can't read stdin", whoami);
	perror (errbuf);
	exit (1);
    } else if (strncmp ((char *) Elfdata, ELFMAG, SELFMAG) != 0) {
	DBUG_PRINT ("elf", ("found a non-ELF format file"));
	fprintf (stderr, "%s: input is not an ELF file\n", whoami);
	exit (1);
    } else {
	DBUG_PRINT ("read", ("got a valid ELF file"));
	Ehdr = (Elf32_Ehdr *) (Elfdata + 0);
	if (Ehdr -> e_type != ET_REL) {
	    fprintf (stderr, "%s: input is not a relocatable file\n", whoami);
	    exit (1);
	}
	if (Ehdr -> e_shoff == 0 || Ehdr -> e_shnum == 0) {
	    fprintf (stderr, "%s: missing section header table!\n", whoami);
	    exit (1);
	} else {
	    Shdr = (Elf32_Shdr *) (Elfdata + Ehdr -> e_shoff);
	}
	i.i_shstrtab = Ehdr -> e_shstrndx;
	shstrtab = Elfdata + (Shdr + i.i_shstrtab) -> sh_offset;
	for (index = 1; index < Ehdr -> e_shnum; index++) {
	    DBUG_PRINT ("shdr", ("examine section header entry %d", index));
	    shdrp = Shdr + index;
	    shdrname = shstrtab + shdrp -> sh_name;
	    DBUG_PRINT ("shdr", ("name is '%s'", shdrname));
	    if (strcmp (".text", shdrname) == 0) {
		i.i_text = index;
	    } else if (strcmp (".rodata", shdrname) == 0) {
		i.i_rodata = index;
	    } else if (strcmp (".data", shdrname) == 0) {
		i.i_data = index;
	    } else if (strcmp (".bss", shdrname) == 0) {
		i.i_bss = index;
	    } else if (strcmp (".symtab", shdrname) == 0) {
		i.i_symtab = index;
	    } else if (strcmp (".strtab", shdrname) == 0) {
		i.i_strtab = index;
	    } else if (strcmp (".rela.text", shdrname) == 0) {
		i.i_rela_text = index;
	    } else if (strcmp (".rela.rodata", shdrname) == 0) {
		i.i_rela_rodata = index;
	    } else if (strcmp (".rela.data", shdrname) == 0) {
		i.i_rela_data = index;
	    }
	}
	if (i.i_text == 0) {
	    fprintf (stderr, "%s: missing '.text' section\n", whoami);
	    exit (1);
	}
	if (i.i_rodata == 0) {
	    /* The read-only data segment is optional */
	}
	if (i.i_data == 0) {
	    fprintf (stderr, "%s: missing '.data' section\n", whoami);
	    exit (1);
	}
	if (i.i_bss == 0) {
	    fprintf (stderr, "%s: missing '.bss' section\n", whoami);
	    exit (1);
	}
	if (i.i_symtab == 0) {
	    fprintf (stderr, "%s: missing '.symtab' section\n", whoami);
	    exit (1);
	}
	if (i.i_strtab == 0) {
	    fprintf (stderr, "%s: missing '.strtab' section\n", whoami);
	    exit (1);
	}
	if (i.i_shstrtab == 0) {
	    fprintf (stderr, "%s: missing '.shstrtab' section\n", whoami);
	    exit (1);
	}
	if (i.i_rela_text == 0) {
	    fprintf (stderr, "%s: missing '.rela.text' section\n", whoami);
	    exit (1);
	}
	if (i.i_rela_rodata == 0) {
	    /* The rodata segment and relocation table are optional */
	}
	if (i.i_rela_data == 0) {
	    fprintf (stderr, "%s: missing '.rela.data' section\n", whoami);
	    exit (1);
	}
	textsize = Shdr[i.i_text].sh_size;
	if (i.i_rodata > 0) {
	    rodatasize = Shdr[i.i_rodata].sh_size;
	}
	datasize = Shdr[i.i_data].sh_size;
	bsssize = Shdr[i.i_bss].sh_size;
    }
    DBUG_VOID_RETURN;
}

/*
 *	Process each symbol table entry, modifying the .text, .data,
 *	or .bss symbols to be relative to the start of a combined,
 *	contiguous section, and allocating COMMON symbols at the end.
 *	Then initialize the values of some special symbols such as
 *	'etext', 'edata', and 'end'.
 *
 */

static void ProcessSymtab ()
{
    unsigned int nsyms;
    unsigned int symidx;
    Elf32_Sym *symtab;
    Elf32_Sym *sym;
    unsigned long end;
    char *symname;

    DBUG_ENTER ("ProcessSymtab");
    nsyms = Shdr[i.i_symtab].sh_size / Shdr[i.i_symtab].sh_entsize;
    DBUG_PRINT ("nsyms", ("process %d symbol table entries", nsyms));
    end = textsize + rodatasize + datasize + bsssize;
    DBUG_PRINT ("end", ("starting value of end is %#lx", end));
    symtab = (Elf32_Sym *) (Elfdata + Shdr[i.i_symtab].sh_offset);
    for (symidx = 0; symidx < nsyms; symidx++) {
	sym = symtab + symidx;
	if (sym -> st_shndx == SHN_COMMON) {
	    sym -> st_shndx = i.i_text;
	    end += (sym -> st_value - 1);
	    end &= ~(sym -> st_value - 1);
	    DBUG_PRINT ("bss", ("alloc sym[%d] at offset %#lx", symidx, end));
	    sym -> st_value = end;
	    end += sym -> st_size;
	    DBUG_PRINT ("end", ("end moved to %#lx", end));
	    bsssize = end - textsize - rodatasize - datasize;
	} else if (sym -> st_shndx == i.i_text) {
	    /* no adjustments needed to text symbols */
	} else if (i.i_rodata && sym -> st_shndx == i.i_rodata) {
	    sym -> st_shndx = i.i_text;
	    sym -> st_value += textsize;
	} else if (sym -> st_shndx == i.i_data) {
	    sym -> st_shndx = i.i_text;
	    sym -> st_value += textsize + rodatasize;
	} else if (sym -> st_shndx == i.i_bss) {
	    sym -> st_shndx = i.i_text;
	    sym -> st_value += (textsize + rodatasize + datasize);
	}
    }
    for (symidx = 0; symidx < nsyms; symidx++) {
	sym = symtab + symidx;
	if (sym -> st_name) {
	    symname = Elfdata + Shdr[i.i_strtab].sh_offset + sym -> st_name;
	    if (strcmp (symname, "etext") == 0) {
		sym -> st_value = textsize;
		sym -> st_shndx = i.i_text;
		DBUG_PRINT ("etext", ("'etext' set to %#lx", sym -> st_value));
	    } else if (strcmp (symname, "edata") == 0) {
		sym -> st_value = textsize + rodatasize + datasize;
		sym -> st_shndx = i.i_text;
		DBUG_PRINT ("edata", ("'edata' set to %#lx", sym -> st_value));
	    } else if (strcmp (symname, "end") == 0) {
		sym -> st_value = textsize + rodatasize + datasize + bsssize;
		sym -> st_shndx = i.i_text;
		DBUG_PRINT ("end", ("'end' set to %#lx", sym -> st_value));
	    }
	} else {
	    symname = "";
	}
	DBUG_PRINT ("symv", ("sym[%d] '%s' %#lx", symidx, symname,
			     sym -> st_value));
    }
    DBUG_VOID_RETURN;
}

/*
 *	Given a pointer to an old relocation table entry and a pointer
 *	to a new relocation table entry, initialize the new relocation
 *	table entry appropriately.
 */

static void DoRelEntry (Elf32_Rela *relp, struct nrel *nrelp)
{
    Elf32_Sym *symtab;		/* Pointer to reference symbol */
    Elf32_Sym *sym;		/* Pointer to reference symbol */
    int reltype;

    DBUG_ENTER ("DoRelEntry");
    symtab = (Elf32_Sym *) (Elfdata + Shdr[i.i_symtab].sh_offset);
    DBUG_PRINT ("rel", ("reloc -> sym[%d]", ELF32_R_SYM (relp -> r_info)));
    sym = symtab + ELF32_R_SYM (relp -> r_info);
    DBUG_PRINT ("rel", ("sym is in section %d", sym -> st_shndx));
    if ((sym -> st_shndx != i.i_text) && (sym -> st_shndx != SHN_ABS)) {
	fprintf (stderr, "%s: reloc uses unbound sym[%d]\n",
		 whoami, sym - symtab);
	exit (1);
    }
    reltype = ELF32_R_TYPE (relp -> r_info);
    nrelp -> nr_offset = relp -> r_offset;
    nrelp -> nr_value = relp -> r_addend;
    if (reltype != R_68K_32) {
	fprintf (stderr, "%s: bad relocation type %u\n", reltype);
	exit (1);
    } else {
	nrelp -> nr_value += sym -> st_value;
	if (sym -> st_shndx == SHN_ABS) {
	    DBUG_PRINT ("rel", ("found an ABSOLUTE symbol"));
	    nrelp -> nr_offset |= 0x80000000;
	}
    }
    DBUG_VOID_RETURN;
}

/*
 *	Generate a combined compact relocation table for the combined
 *	.text and .data sections.  Note that the offset for the new
 *	relocation table entries, derived from the old .data relocation
 *	entries, has to be adjusted by the size of the .text section
 *	since they will be combined at output.
 */

static void GenRelocTable ()
{
    unsigned long ntrel;	/* Number of text relocation entries */
    unsigned long nrodrel;	/* Number of rodata relocation entries */
    unsigned long ndrel;	/* Number of data relocation entries */
    struct nrel *nreltabp;	/* Pointer into new relocation table */
    Elf32_Rela *oreltabp;	/* Pointer into old relocation table */
    extern char *malloc ();

    DBUG_ENTER ("GenRelocTable");
    ntrel = Shdr[i.i_rela_text].sh_size / Shdr[i.i_rela_text].sh_entsize;
    DBUG_PRINT ("rel", ("found %lu text relocations", ntrel));
    if (i.i_rela_rodata > 0) {
	nrodrel = Shdr[i.i_rela_rodata].sh_size / Shdr[i.i_rela_rodata].sh_entsize;
	DBUG_PRINT ("rel", ("found %lu rodata relocations", nrodrel));
    }
    ndrel = Shdr[i.i_rela_data].sh_size / Shdr[i.i_rela_data].sh_entsize;
    DBUG_PRINT ("rel", ("found %lu data relocations", ndrel));
    nrelcount = ntrel + nrodrel + ndrel;
    nreltab = (struct nrel *) malloc (nrelcount * sizeof (struct nrel));
    nreltabp = nreltab;
    oreltabp = (Elf32_Rela *) (Elfdata + Shdr[i.i_rela_text].sh_offset);
    for (; ntrel-- != 0; oreltabp++, nreltabp++) {
	DoRelEntry (oreltabp, nreltabp);
    }
    if (nrodrel > 0) {
	oreltabp = (Elf32_Rela *) (Elfdata + Shdr[i.i_rela_rodata].sh_offset);
	for (; nrodrel-- != 0; oreltabp++, nreltabp++) {
	    DoRelEntry (oreltabp, nreltabp);
	    nreltabp -> nr_offset += textsize;
	}
    }
    oreltabp = (Elf32_Rela *) (Elfdata + Shdr[i.i_rela_data].sh_offset);
    for (; ndrel-- != 0; oreltabp++, nreltabp++) {
	DoRelEntry (oreltabp, nreltabp);
	nreltabp -> nr_offset += textsize + rodatasize;
    }
    DBUG_VOID_RETURN;
}

/*
 *	Now put all the pieces together and write out a new ELF file.
 *	The only parts we need are the ELF header, the combined .text
 *	and .data section (as a single .text section), a section header
 *	string table, a combined compact relocation table, and a section
 *	header table.  Some of the appropriate information is hardcoded
 *	into the following code; not pretty but works for now.
 *
 *	The file type is changed from ET_REL to ET_LOPROC, to discourage
 *	standard tools from thinking it is a normal relocatable file.
 */

static char newshstrtab[] = ".text\000.shstrtab\000.rel.boot\000\000";

#define TEXTSHOFF	0	/* magic offsets into above new shstrtab */
#define SHSTRTABOFF	6	/* magic offsets into above new shstrtab */
#define RELBOOTOFF	16	/* magic offsets into above new shstrtab */

static void WriteElfFile ()
{
    DBUG_ENTER ("WriteElfFile");
    Ehdr -> e_type = ET_LOPROC;
    Ehdr -> e_entry = 0;
    Ehdr -> e_phoff = 0;
    Ehdr -> e_shoff = sizeof (Elf32_Ehdr) + textsize + rodatasize + datasize;
    Ehdr -> e_shoff += sizeof (newshstrtab) + nrelcount * sizeof (struct nrel);
    Ehdr -> e_phentsize = 0;
    Ehdr -> e_phnum = 0;
    Ehdr -> e_shnum = 4;
    Ehdr -> e_shstrndx = 2;
    write (1, (char *) Ehdr, sizeof (Elf32_Ehdr));
    write (1, (char *) (Elfdata + Shdr[i.i_text].sh_offset), textsize);
    if (rodatasize > 0) {
	write (1, (char *) (Elfdata + Shdr[i.i_rodata].sh_offset), rodatasize);
    }
    write (1, (char *) (Elfdata + Shdr[i.i_data].sh_offset), datasize);
    write (1, (char *) newshstrtab, sizeof (newshstrtab));
    write (1, (char *) nreltab, nrelcount * sizeof (struct nrel));
    write (1, (char *) Shdr, sizeof (Elf32_Shdr));
    Shdr[i.i_text].sh_name = 0;
    Shdr[i.i_text].sh_offset = sizeof (Elf32_Ehdr);
    Shdr[i.i_text].sh_size = textsize + rodatasize + datasize;
    write (1, (char *) (Shdr + i.i_text), sizeof (Elf32_Shdr));
    Shdr[i.i_shstrtab].sh_name = SHSTRTABOFF;
    Shdr[i.i_shstrtab].sh_offset = Shdr[i.i_text].sh_offset;
    Shdr[i.i_shstrtab].sh_offset += Shdr[i.i_text].sh_size;
    Shdr[i.i_shstrtab].sh_size = sizeof (newshstrtab);
    write (1, (char *) (Shdr + i.i_shstrtab), sizeof (Elf32_Shdr));
    Shdr[i.i_rela_text].sh_name = RELBOOTOFF;
    Shdr[i.i_rela_text].sh_type = SHT_LOUSER;
    Shdr[i.i_rela_text].sh_offset = Shdr[i.i_shstrtab].sh_offset;
    Shdr[i.i_rela_text].sh_offset += Shdr[i.i_shstrtab].sh_size;
    Shdr[i.i_rela_text].sh_link = 0;
    Shdr[i.i_rela_text].sh_info = 1;
    Shdr[i.i_rela_text].sh_size = nrelcount * sizeof (struct nrel);
    Shdr[i.i_rela_text].sh_entsize = sizeof (struct nrel);
    write (1, (char *) (Shdr + i.i_rela_text), sizeof (Elf32_Shdr));
    DBUG_VOID_RETURN;
}
