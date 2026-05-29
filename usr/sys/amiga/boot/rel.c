/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 *      Patchlevel: rel.c_1.1
 *
 *	Given a pointer to an ELF file that has been loaded into
 *	memory, and a pointer to a structure which contains, among
 *	other things, the virtual address to which the start of text
 *	is to be bound, performs relocation for the text and data
 *	segments, and fills in other fields in the passed ELF file
 *	info structure.
 *
 *	The text segment is bound to the specified starting virtual
 *	address, followed immediately by initialized data, and then
 *	uninitialized data.  Certain assumptions are made about the
 *	existance and ordering of the text, data, and bss sections.
 *	There are other limitations, such as only handling the
 *	minimal number of relocation types necessary.
 *
 */

#if (DBUG || TEST)
#  include <stdio.h>
#endif

#if DBUG
#  include <local/dbug.h>
#else
#  define DBUG_ENTER(a1)
#  define DBUG_RETURN(a1) return(a1)
#  define DBUG_VOID_RETURN return
#  define DBUG_EXECUTE(keyword,a1)
#  define DBUG_PRINT(keyword,arglist)
#  define DBUG_PUSH(a1)
#  define DBUG_POP()
#  define DBUG_PROCESS(a1)
#  define DBUG_FILE (stderr)
#  define DBUG_SETJMP setjmp
#  define DBUG_LONGJMP longjmp
#endif

#include <elf.h>
#include <sys/elf_68K.h>

#include "bootdata.h"

#if TEST
#  define COMPLAIN(fmt,arg)	fprintf(stderr,fmt,arg)
#  define RELOC(string)		string
#else
#  pragma pack(2)
#  include <exec/alerts.h>
#  pragma pack(4)
#  define NULL			0
#  define COMPLAIN(fmt,arg)	Alert(0x52454C41|AT_DeadEnd,0x4)
#  define RELOC(string)		reloc(string)
#endif

#if !TEST
extern char *reloc(char *arg);
#endif

extern int streq (char *s1, char *s2);
extern int streqn (char *s1, char *s2, int nbytes);

/*
 *  Because we can't have any static variables (must be PIC), we
 *  simply collect all the global variables into a structure an
 *  pass around a pointer to it.
 */

struct globals {
    char *elfdata;		/* Pointer to in memory file copy */
    Elf32_Ehdr *Ehdr;		/* Pointer to ELF file header */
    Elf32_Shdr *Shdrs;		/* Pointer to ELF section header table */
    Elf32_Phdr *Phdrs;		/* Pointer to ELF program header table */
    Elf32_Shdr *thdr;		/* Pointer to text section header */
    Elf32_Shdr *rhdr;		/* Pointer to rodata section header */
    Elf32_Shdr *dhdr;		/* Pointer to data section header */
    Elf32_Shdr *bhdr;		/* Pointer to bss section header */
    Elf32_Shdr *symhdr;		/* Pointer to symbol table section header */
    Elf32_Addr end;		/* Current end of text+data+bss */
};

struct nrel {
    unsigned long nr_offset;
    unsigned long nr_value;
};

static Elf32_Addr symvaddr (struct globals *gbls, Elf32_Sym *sym,
			    unsigned int symidx)
{
    Elf32_Shdr *rshdr;
    Elf32_Addr vaddr = 0;

    DBUG_ENTER ("symvaddr");
    if (sym -> st_shndx > 0 && sym -> st_shndx < gbls -> Ehdr -> e_shnum) {
	DBUG_PRINT ("symdef", ("defined in section %d", sym -> st_shndx));
	rshdr = gbls -> Shdrs + sym -> st_shndx;
	if (rshdr -> sh_addr == 0) {
	    COMPLAIN ("symbol[%d] is", symidx);
	    COMPLAIN (" in unbound section %d\n", sym -> st_shndx);
	} else {
	    vaddr = rshdr -> sh_addr + sym -> st_value;
	}
    } else if (sym -> st_shndx == SHN_ABS) {
	DBUG_PRINT ("abssym", ("is an absolute symbol"));
	vaddr = sym -> st_value;
    } else {
	COMPLAIN ("symbol[%d]", symidx);
	COMPLAIN (" references section %d\n", sym -> st_shndx);
    }
    DBUG_PRINT ("symval", ("symbol[%d] value is %#lx", symidx, vaddr));
    DBUG_RETURN (vaddr);
}

static int relocsection (struct globals *gbls, Elf32_Shdr *shdr,
			 Elf32_Shdr *relsechdr, Elf32_Shdr *symtabhdr)
{
    char *secdata;
    char *datap;
    Elf32_Sym *syms;
    Elf32_Sym *sym;
    Elf32_Rela *relp;
    Elf32_Addr vaddr;
    unsigned int nrel;
    int reltype;
    unsigned int symidx;

    
    DBUG_ENTER ("relocsection");
    secdata = (char *) (gbls -> elfdata + relsechdr -> sh_offset);
    syms = (Elf32_Sym *) (gbls -> elfdata + symtabhdr -> sh_offset);
    relp = (Elf32_Rela *) (gbls -> elfdata + shdr -> sh_offset);
    nrel = shdr -> sh_size / shdr -> sh_entsize;
    DBUG_PRINT ("nrel", ("section has %u relocation records", nrel));    
    while (nrel-- > 0) {
	reltype = ELF32_R_TYPE (relp -> r_info);
	symidx = ELF32_R_SYM (relp -> r_info);
	sym = syms + symidx;
	datap = secdata + relp -> r_offset;
	vaddr = symvaddr (gbls, sym, symidx);
	switch (reltype) {
	    case R_68K_32:
		*(long *)datap = vaddr + relp -> r_addend;
		DBUG_PRINT ("reloc", ("reloc to %#lx", *(long *)datap));
		break;
	    default:
		COMPLAIN ("unknown relocation type %d\n", reltype);
		break;
	}
	relp++;
    }
    DBUG_RETURN (0);
}

static int allocbss (struct globals *gbls)
{
    Elf32_Sym *syms;
    Elf32_Sym *sym;
    Elf32_Addr vaddr;
    unsigned int nsyms;
    unsigned int symidx;

    DBUG_ENTER ("allocbss");
    nsyms = gbls -> symhdr -> sh_size / gbls -> symhdr -> sh_entsize;
    DBUG_PRINT ("nsyms", ("found %#x symbols", nsyms));
    syms = (Elf32_Sym *) (gbls -> elfdata + gbls -> symhdr -> sh_offset);
    for (symidx = 0; symidx < nsyms; symidx++) {
	sym = syms + symidx;
	if (sym -> st_shndx == SHN_COMMON) {
	    if (gbls -> bhdr == NULL) {
		COMPLAIN ("symbol[%d] in missing bss section\n", symidx);
		break;
	    } else {
		sym -> st_shndx = gbls -> bhdr - gbls -> Shdrs;
		vaddr = gbls -> bhdr -> sh_addr + gbls -> bhdr -> sh_size;
		vaddr += (sym -> st_value - 1);
		vaddr &= ~(sym -> st_value - 1);
		DBUG_PRINT ("bss", ("alloc sym[%d] at bss vaddr %#lx",
				    symidx, vaddr));
		sym -> st_value = vaddr - gbls -> bhdr -> sh_addr;
		gbls -> end = vaddr + sym -> st_size;
		DBUG_PRINT ("end", ("end moved to %#lx", gbls -> end));
		gbls -> bhdr -> sh_size = gbls -> end - gbls -> bhdr -> sh_addr;
	    }
	}
    }
    DBUG_RETURN (0);
}

static int specialsyms (struct globals *gbls)
{
    Elf32_Sym *syms;
    Elf32_Sym *sym;
    Elf32_Addr vaddr;
    unsigned int nsyms;
    unsigned int symidx;
    Elf32_Shdr *strtabhdr;
    char *strtab;
    char *symname;

    DBUG_ENTER ("specialsyms");
    nsyms = gbls -> symhdr -> sh_size / gbls -> symhdr -> sh_entsize;
    DBUG_PRINT ("nsyms", ("found %#x symbols", nsyms));
    syms = (Elf32_Sym *) (gbls -> elfdata + gbls -> symhdr -> sh_offset);
    strtabhdr = gbls -> Shdrs + gbls -> symhdr -> sh_link;
    strtab = gbls -> elfdata + strtabhdr -> sh_offset;
    for (symidx = 0; symidx < nsyms; symidx++) {
	sym = syms + symidx;
	if (sym -> st_name) {
	    symname = strtab + sym -> st_name;
	    DBUG_PRINT ("sym", ("symbol[%d] = '%s'", symidx, symname));
	    if (streq (symname, RELOC ("etext"))) {
		sym -> st_value = gbls -> thdr -> sh_addr
		    + gbls -> thdr -> sh_size;
		sym -> st_shndx = SHN_ABS;
	    } else if (streq (symname, RELOC ("edata"))) {
		sym -> st_value = gbls -> dhdr -> sh_addr
		    + gbls -> dhdr -> sh_size;
		sym -> st_shndx = SHN_ABS;
	    } else if (streq (symname, RELOC ("end"))) {
		sym -> st_value = gbls -> end;
		sym -> st_shndx = SHN_ABS;
	    }
	}
    }
    DBUG_RETURN (0);
}

/*
 *	Look through the section header table and pick out the sections
 *	headers for the ".text", ".rodata" (if any), and ".data" sections.
 *
 *	Note that if we are missing text or data, we are in deep trouble.
 *	The text section must also be first.  The read-only data is optional.
 */

static int findsections (struct globals *gbls, struct BootData *bip)
{
    unsigned int sectnum;
    Elf32_Shdr *shdr;
    Elf32_Shdr *strtabhdr;
    char *strtab;
    char *sectname;
    
    DBUG_ENTER ("findsections");
    DBUG_PRINT ("sects", ("look at %d sections", gbls -> Ehdr -> e_shnum));
    strtabhdr = gbls -> Shdrs + gbls -> Ehdr -> e_shstrndx;
    strtab = gbls -> elfdata + strtabhdr -> sh_offset;
    for (sectnum = 0; sectnum < gbls -> Ehdr -> e_shnum; sectnum++) {
	shdr = gbls -> Shdrs + sectnum;
	sectname = strtab + shdr -> sh_name;
	DBUG_PRINT ("sect", ("section[%d] '%s'", sectnum, sectname));
	if (shdr -> sh_type == SHT_PROGBITS) {
	    if ((shdr -> sh_size > 0) && (shdr -> sh_flags & SHF_ALLOC)) {
		if (shdr -> sh_flags & SHF_EXECINSTR) {
		    gbls -> thdr = shdr;
		} else if (shdr -> sh_flags & SHF_WRITE) {
		    gbls -> dhdr = shdr;
		} else {
		    gbls -> rhdr = shdr;
		}
	    }
	} else if (shdr -> sh_type == SHT_NOBITS) {
	    if (shdr -> sh_flags & SHF_WRITE) {
		if (shdr -> sh_size > 0) {
		    gbls -> bhdr = shdr;
		}
	    }
	} else if (shdr -> sh_type == SHT_SYMTAB) {
	    gbls -> symhdr = shdr;
	}
    }
    DBUG_RETURN (0);
}

/*
 *	Assign base addresses to the .text, .rodata, .data, and .bss
 *	sections, in that order.  Also initialize the boot info fields
 *	so that the section can be found and copied to its final
 *	execution location.
 */

static int bindsections (struct globals *gbls, struct BootData *bip)
{
    DBUG_ENTER ("bindsections");
    if (gbls -> thdr != NULL) {
	gbls -> thdr -> sh_addr = gbls -> end;
	gbls -> end += gbls -> thdr -> sh_size;
	DBUG_PRINT ("end", ("end = %#lx", gbls -> end));
	DBUG_PRINT ("text", ("text @ %#lx", gbls -> thdr -> sh_addr));
	DBUG_PRINT ("text", ("text size %#lx", gbls -> thdr -> sh_size));
	bip -> bd_tvaddr = gbls -> thdr -> sh_addr;
	bip -> bd_toffset = gbls -> thdr -> sh_offset;
	bip -> bd_tsize = gbls -> thdr -> sh_size;
    }
    if (gbls -> rhdr != NULL) {
	gbls -> rhdr -> sh_addr = gbls -> end;
	gbls -> end += gbls -> rhdr -> sh_size;
	DBUG_PRINT ("end", ("end = %#lx", gbls -> end));
	DBUG_PRINT ("rodata", ("rodata @ %#lx", gbls -> rhdr -> sh_addr));
	DBUG_PRINT ("rodata", ("rodata size %#lx", gbls -> rhdr -> sh_size));
	bip -> bd_rvaddr = gbls -> rhdr -> sh_addr;
	bip -> bd_roffset = gbls -> rhdr -> sh_offset;
	bip -> bd_rsize = gbls -> rhdr -> sh_size;
    }	
    if (gbls -> dhdr != NULL) {
	gbls -> dhdr -> sh_addr = gbls -> end;
	gbls -> end += gbls -> dhdr -> sh_size;
	DBUG_PRINT ("end", ("end = %#lx", gbls -> end));
	DBUG_PRINT ("data", ("data @ %#lx", gbls -> dhdr -> sh_addr));
	DBUG_PRINT ("data", ("data size %#lx", gbls -> dhdr -> sh_size));
	bip -> bd_dvaddr = gbls -> dhdr -> sh_addr;
	bip -> bd_doffset = gbls -> dhdr -> sh_offset;
	bip -> bd_dsize = gbls -> dhdr -> sh_size;
    }
    if (gbls -> bhdr != NULL) {
	gbls -> bhdr -> sh_addr = gbls -> end;
	gbls -> end += gbls -> bhdr -> sh_size;
	DBUG_PRINT ("end", ("end = %#lx", gbls -> end));
	DBUG_PRINT ("bss", ("bss @ %#lx", gbls -> bhdr -> sh_addr));
	DBUG_PRINT ("bss", ("bss size %#lx", gbls -> bhdr -> sh_size));
    }
    DBUG_RETURN (0);
}

static int relocsections (struct globals *gbls, struct BootData *bip)
{
    unsigned int sectnum;
    Elf32_Shdr *shdr;
    Elf32_Shdr *symtabhdr;
    Elf32_Shdr *relsechdr;

    DBUG_ENTER ("relocsections");
    for (sectnum = 0; sectnum < gbls -> Ehdr -> e_shnum; sectnum++) {
	shdr = gbls -> Shdrs + sectnum;
	DBUG_PRINT ("sectoff", ("sect @ offset %#lx", shdr -> sh_offset));
	if (shdr -> sh_type == SHT_RELA) {
	    DBUG_PRINT ("rela", ("found relocation section"));
	    symtabhdr = gbls -> Shdrs + shdr -> sh_link;
	    relsechdr = gbls -> Shdrs + shdr -> sh_info;
	    relocsection (gbls, shdr, relsechdr, symtabhdr);
	}	
    }
    DBUG_RETURN (0);
}

static void compactreloc (struct globals *gbls)
{
    Elf32_Shdr *relshdr;
    struct nrel *nrelp;
    unsigned int nrelcount;
    char *textbase;
    unsigned long relvalue;
    unsigned long offset;

    DBUG_ENTER ("compactreloc");
    relshdr = gbls -> Shdrs + 3;
    nrelcount = relshdr -> sh_size / relshdr -> sh_entsize;
    DBUG_PRINT ("nrelcount", ("found %d reloc entries", nrelcount));
    nrelp = (struct nrel *) (gbls -> elfdata + relshdr -> sh_offset);
    textbase = gbls -> elfdata + gbls -> thdr -> sh_offset;
    while (nrelcount-- != 0) {
	relvalue = nrelp -> nr_value;
	offset = nrelp -> nr_offset;
	if (!(offset & 0x80000000)) {
	    relvalue += gbls -> thdr -> sh_addr;
	}
	DBUG_PRINT ("rel", ("text[%x] = %x", offset & 0x7FFFFFFF, relvalue));
	*(long *)(textbase + (offset & 0x7FFFFFFF)) = relvalue;
	nrelp++;
    }
    DBUG_VOID_RETURN;
}

/*
 *	Note that if the ELF file is fully executable (all relocation
 *	already done, the first entry in the program header table is
 *	presumed to be the entry for the combined text, read-only data,
 *	and data segments.  This is true with the current development
 *	tools and procedures, but is not required by the ELF format.
 *	Should be fixed.
 */

int rel (char *elfdata, Elf32_Addr vaddr, struct BootData *bip)
{
    struct globals gblvars;
    Elf32_Shdr *shdr;
    int rtnval = 1;

    DBUG_ENTER ("rel");
    DBUG_PRINT ("vaddr", ("text starts at %#lx", vaddr));
    gblvars.Ehdr = (Elf32_Ehdr *) elfdata;
    bip -> bd_tvaddr = 0;
    bip -> bd_toffset = 0;
    bip -> bd_tsize = 0;
    bip -> bd_rvaddr = 0;
    bip -> bd_roffset = 0;
    bip -> bd_rsize = 0;
    bip -> bd_dvaddr = 0;
    bip -> bd_doffset = 0;
    bip -> bd_dsize = 0;
    if (!streqn ((char *) (gblvars.Ehdr -> e_ident), RELOC (ELFMAG), SELFMAG)) {
	DBUG_PRINT ("elf", ("found a non-ELF format file"));
	COMPLAIN ("found a non-ELF format file\n", 0);
	rtnval = 0;	
    } else if (gblvars.Ehdr -> e_type == ET_EXEC) {
	DBUG_PRINT ("elf", ("found an executable, fully linked file"));
	gblvars.Phdrs = (Elf32_Phdr *) (elfdata + gblvars.Ehdr -> e_phoff);
	bip -> bd_entry = gblvars.Ehdr -> e_entry;
	bip -> bd_tvaddr = gblvars.Phdrs -> p_vaddr;
	bip -> bd_toffset = gblvars.Phdrs -> p_offset;
	bip -> bd_tsize = gblvars.Phdrs -> p_filesz;
    } else if (gblvars.Ehdr -> e_type == ET_LOPROC) {
	DBUG_PRINT ("elf", ("found a compact relocatable ELF file"));
	gblvars.elfdata = elfdata;
	gblvars.Shdrs = (Elf32_Shdr *) (gblvars.elfdata + gblvars.Ehdr -> e_shoff);
	gblvars.thdr = gblvars.Shdrs + 1;
	gblvars.thdr -> sh_addr = vaddr;
	bip -> bd_entry = vaddr;
	bip -> bd_tvaddr = vaddr;
	bip -> bd_toffset = gblvars.thdr -> sh_offset;
	bip -> bd_tsize = gblvars.thdr -> sh_size;
	compactreloc (&gblvars);
    } else if (gblvars.Ehdr -> e_type == ET_REL) {
	DBUG_PRINT ("elf", ("found a relocatable ELF file"));
	bip -> bd_entry = vaddr;
	gblvars.elfdata = elfdata;
	gblvars.Shdrs = (Elf32_Shdr *) (gblvars.elfdata + gblvars.Ehdr -> e_shoff);
	gblvars.thdr = NULL;
	gblvars.rhdr = NULL;
	gblvars.dhdr = NULL;
	gblvars.bhdr = NULL;
	gblvars.end = vaddr;
	findsections (&gblvars, bip);
	bindsections (&gblvars, bip);
	allocbss (&gblvars);
	specialsyms (&gblvars);
	relocsections (&gblvars, bip);
    } else {
	COMPLAIN ("ELF e_file type %d not handled\n", gblvars.Ehdr -> e_type);
	rtnval = 0;
    }
    DBUG_RETURN (rtnval);
}

