/* Copyright (C) 1991, Commodore Business Machines, Inc. */

#define MSG_ELFHDR	1	/* can't read ELF header */
#define MSG_ELFMAGIC	2	/* bad ELF magic number, not an ELF file */
#define MSG_MPHT	3	/* missing program header table */
#define MSG_PHTSEEK	4	/* can't seek to program header table */
#define MSG_BADPHT	5	/* bad program header table structure */
#define MSG_PHTREAD	6	/* can't read program header table entry */
#define MSG_LDSKIP	7	/* warning - loadable segment %u skipped */
#define MSG_TOOBIG	8	/* not enough space in bootblocks */
#define MSG_BOOTSEEK	9	/* can't seek to boot segment in ELF file */
#define MSG_BOOTREAD	10	/* can't read boot segment from ELF file */
#define MSG_BOOTWRITE	11	/* can't write boot image */
#define MSG_LAYOUT	12
#define MSG_PHTLOAD	13	/* program segment not loadable */
#define MSG_MALLOC	14	/* malloc failed */
#define MSG_BOOTSIZE	15	/* bootsize must be multiple of 1K */
