/*
** hydra_probe — user-space Hydra AmigaNet board probe for AMIX
**
** Usage:
**   ./hydra_probe                 quick scan, report only candidates
**   ./hydra_probe -r              raw scan — show EVERY slot
**   ./hydra_probe -a <hexaddr>    probe a single address
**   ./hydra_probe -v              verbose
**
** Scans Zorro II memory via /dev/mem + mmap(). After Kickstart
** configures the boards, the AutoConfig ROM space (0xE80000-)
** is dead.  The board's real registers live at its assigned
** 64 KB window somewhere in Zorro II space.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/errno.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define NIC_OFFSET      0xFFE1
#define PROM_OFFSET     0xFFC0

#define NE_CR           0x00
#define NE_ISR          0x07
#define NE_DCR          0x0E

static int mem_fd = -1;

static void
die(const char *msg)
{
    perror(msg);
    exit(1);
}

static volatile unsigned char *
map_phys(unsigned long pa, int size)
{
    volatile unsigned char *p;
    long mask = PAGE_SIZE - 1;
    unsigned long base = pa & ~mask;
    unsigned long off  = pa &  mask;

    p = (volatile unsigned char *)mmap(0, off + size,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED, mem_fd, base);
    if (p == MAP_FAILED)
        return NULL;
    return p + off;
}

static void
unmap_phys(volatile unsigned char *p, int size)
{
    if (p) {
        long mask = PAGE_SIZE - 1;
        unsigned long a = (unsigned long)p;
        unsigned long base = a & ~mask;
        unsigned long off  = a &  mask;
        munmap((void *)base, off + size);
    }
}

static unsigned char
nic_read(volatile unsigned char *nic, int reg)
{
    return *(nic + (reg * 2));
}

/* ---- probe one address ---- */

static void
probe_one(unsigned long addr, const char *label)
{
    volatile unsigned char *nic, *prom;
    unsigned char cr, isr, dcr, mac[6];
    int i, prom_ok;

    nic = map_phys(addr + NIC_OFFSET, 16);
    if (!nic) {
        printf("%s 0x%08lx  mmap NIC FAILED\n", label, addr);
        return;
    }

    cr  = nic_read(nic, NE_CR);
    isr = nic_read(nic, NE_ISR);
    dcr = nic_read(nic, NE_DCR);
    unmap_phys(nic, 16);

    /* Bus-float check */
    if (cr == 0xFF && isr == 0xFF && dcr == 0xFF) {
        printf("%s 0x%08lx  all-0xFF (bus float / empty slot)\n",
               label, addr);
        return;
    }
    if (cr == 0x00 && isr == 0x00 && dcr == 0x00) {
        printf("%s 0x%08lx  all-0x00 (bus float / empty slot)\n",
               label, addr);
        return;
    }

    /* Something is here — print NIC regs */
    printf("%s 0x%08lx  CR=0x%02x ISR=0x%02x DCR=0x%02x",
           label, addr, cr, isr, dcr);

    /* Try PROM */
    prom = map_phys(addr + PROM_OFFSET, 12);
    if (prom) {
        for (i = 0; i < 6; i++)
            mac[i] = *(prom + i * 2);
        prom_ok = 1;
        /* Quick sanity: valid MAC != 0xFF:FF:FF:FF:FF:FF or 00:00:00:00:00:00 */
        for (i = 0; i < 6; i++)
            if (mac[i] != 0xFF) break;
        if (i == 6) prom_ok = 0;
        for (i = 0; i < 6; i++)
            if (mac[i] != 0x00) break;
        if (i == 6) prom_ok = 0;

        if (prom_ok)
            printf("  MAC %02x:%02x:%02x:%02x:%02x:%02x",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        else
            printf("  MAC %02x:%02x:%02x:%02x:%02x:%02x (INVALID)",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        unmap_phys(prom, 12);
    }

    printf("\n");
}

/* ---- main ---- */

int
main(int argc, char **argv)
{
    int ch, raw = 0, verbose = 0;
    unsigned long single = 0;
    int has_single = 0;
    unsigned long addr;

    while ((ch = getopt(argc, argv, "vra:h")) != EOF) {
        switch (ch) {
        case 'v': verbose = 1; break;
        case 'r': raw = 1; break;
        case 'a':
            single = strtoul(optarg, NULL, 16);
            has_single = 1;
            break;
        default:
            printf("Usage: %s [-r] [-a <hex>]\n", argv[0]);
            printf("  -r        raw scan — show every slot\n");
            printf("  -a <hex>  probe single address\n");
            printf("  -v        verbose\n");
            return 0;
        }
    }

    mem_fd = open("/dev/mem", O_RDWR);
    if (mem_fd < 0)
        die("open /dev/mem");

    printf("Hydra AmigaNet Probe\n");

    if (has_single) {
        probe_one(single, "manual");
    } else {
        /* I/O space: 0xE90000 - 0xEFFFFF, 64 KB slots */
        printf("Zorro II I/O space:\n");
        for (addr = 0x00E90000UL; addr < 0x00F00000UL;
             addr += 0x00010000UL) {
            if (raw) {
                probe_one(addr, "I/O");
            } else {
                volatile unsigned char *nic;
                unsigned char cr, isr, dcr;
                nic = map_phys(addr + NIC_OFFSET, 8);
                if (nic) {
                    cr  = nic_read(nic, NE_CR);
                    isr = nic_read(nic, NE_ISR);
                    dcr = nic_read(nic, NE_DCR);
                    unmap_phys(nic, 8);
                    if (!(cr == 0xFF && isr == 0xFF && dcr == 0xFF) &&
                        !(cr == 0x00 && isr == 0x00 && dcr == 0x00))
                        probe_one(addr, "I/O");
                }
            }
        }

        /* Memory space: 0x200000 - 0x9FFFFF, 64 KB slots */
        printf("Zorro II memory space:\n");
        for (addr = 0x00200000UL; addr < 0x00A00000UL;
             addr += 0x00010000UL) {
            if (raw) {
                probe_one(addr, "MEM");
            } else {
                volatile unsigned char *nic;
                unsigned char cr, isr, dcr;
                nic = map_phys(addr + NIC_OFFSET, 8);
                if (nic) {
                    cr  = nic_read(nic, NE_CR);
                    isr = nic_read(nic, NE_ISR);
                    dcr = nic_read(nic, NE_DCR);
                    unmap_phys(nic, 8);
                    if (!(cr == 0xFF && isr == 0xFF && dcr == 0xFF) &&
                        !(cr == 0x00 && isr == 0x00 && dcr == 0x00))
                        probe_one(addr, "MEM");
                }
            }
        }
    }

    close(mem_fd);
    return 0;
}
