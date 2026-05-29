/*
 *	Copyright (C) 1991, Commodore Business Machines, Inc.
 */

struct infoblock {
    char	ib_ident[8];		/* Identification ('IBLK\0') */
    Elf32_Word	ib_size;		/* Size of the following kernel */
    Elf32_Word	ib_fullsize;		/* Size of a decompressed kernel */
    Elf32_Word	ib_chksum;		/* SVR4 "sum" compatible checksum */
    Elf32_Addr	ib_bind;		/* Text binding address */
    char	ib_pad[512-24];
};

