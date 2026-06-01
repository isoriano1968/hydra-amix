/*
 * zorro_probe.c - Zorro II Expansion Device Probe
 *
 * Detects expansion boards using mmap on /dev/mem (like lszorro),
 * decodes AutoConfig ROM nibble format, and probes STREAMS drivers.
 *
 * Compile:  gcc -O -o zorro_probe zorro_probe.c
 * Run:      ./zorro_probe
 *
 * Must run as root (/dev/mem access).
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>

/* Zorro II I/O slots: 0xE90000 - 0xEFFFFF, 8 x 64KB (same as lszorro) */
#define ZII_IO_BASE   0x00E90000UL
#define ZII_IO_END    0x00EFFFFFUL
#define ZII_IO_STEP   0x00010000UL

/* Zorro II memory area: 0x200000 - 0x9FFFFF, 64KB steps */
#define ZII_MEM_BASE  0x00200000UL
#define ZII_MEM_END   0x009FFFFFUL
#define ZII_MEM_STEP  0x00010000UL

/* Size to map for AutoConfig ROM read */
#define AC_MAP_SIZE   0x80

/* AutoConfig logical byte positions (nibble-encoded) */
#define AC_TYPE    0
#define AC_PRODUCT 1
#define AC_FLAGS   2
#define AC_RESV    3
#define AC_MANUF   4
#define AC_SERIAL  6
#define AC_DIAGVEC 10

/* er_Type bit masks */
#define ERT_TYPEMASK  0xC0
#define ERT_ZORROII   0xC0
#define ERT_MEMLIST   0x20
#define ERT_DIAGVALID 0x10
#define ERT_CHAINED   0x08
#define ERT_SIZEMASK  0x07

/* NE2000 MAC PROM location */
#define NE8390_ADDRPROM_OFFSET 0xffc0

/* Known board IDs */
#define ID_A2065      0x02020070UL  /* Commodore A2065, mfgr 0x0202 prod 112 */
#define ID_HYDRA      0x08490001UL  /* Hydra Systems AmigaNet (2121 / 1) */

#define MAX_BOARDS 32

/* Board size indexed by er_Type bits 2-0 */
static unsigned long size_table[8] = {
    8UL * 1024UL * 1024UL,
    64UL * 1024UL,
    128UL * 1024UL,
    256UL * 1024UL,
    512UL * 1024UL,
    1UL * 1024UL * 1024UL,
    2UL * 1024UL * 1024UL,
    4UL * 1024UL * 1024UL
};

/* Signal handling for bus errors on mmap'd regions */
static jmp_buf env;
static volatile unsigned char *map_fault_addr;

static void bus_handler(int sig)
{
    longjmp(env, 1);
}

/* ------------------------------------------------------------------ */
/* AutoConfig nibble decoder (from lszorro.c)                          */
/* ------------------------------------------------------------------ */

static unsigned char
ac_byte(unsigned char *mem, int n)
{
    unsigned char hi, lo, raw;
    hi  = (mem[n * 4    ] >> 4) & 0x0F;
    lo  = (mem[n * 4 + 2] >> 4) & 0x0F;
    raw = (unsigned char)((hi << 4) | lo);
    return (n == 0) ? raw : (unsigned char)(~raw & 0xFF);
}

static unsigned short
ac_word(unsigned char *mem, int n)
{
    return (unsigned short)(
        ((unsigned short)ac_byte(mem, n) << 8) | ac_byte(mem, n + 1));
}

static unsigned long
ac_long(unsigned char *mem, int n)
{
    return ((unsigned long)ac_word(mem, n) << 16) | ac_word(mem, n + 2);
}

static int
is_autoconfig(unsigned char *mem)
{
    unsigned char er_type;
    int i;

    /* Reject bus float: all 0xFF or all 0x00 */
    for (i = 0; i < 16; i++)
        if (mem[i] != 0xFF) break;
    if (i == 16) return 0;
    for (i = 0; i < 16; i++)
        if (mem[i] != 0x00) break;
    if (i == 16) return 0;

    er_type = ac_byte(mem, AC_TYPE);
    if ((er_type & ERT_TYPEMASK) != ERT_ZORROII)
        return 0;

    /* Reserved byte must decode to 0x00 */
    if (ac_byte(mem, AC_RESV) != 0x00)
        return 0;

    return 1;
}

static const char *
size_str(unsigned long sz)
{
    static char buf[16];
    if (sz >= 1024UL * 1024UL)
        sprintf(buf, "%luMB", sz / (1024UL * 1024UL));
    else
        sprintf(buf, "%luKB", sz / 1024UL);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Board identification                                                */
/* ------------------------------------------------------------------ */

static const char *
identify_board(unsigned short manuf, unsigned char prod)
{
    unsigned long id = ((unsigned long)manuf << 16) | prod;
    switch (id) {
    case ID_A2065:      return "Commodore A2065 Ethernet";
    case ID_HYDRA:      return "Hydra Systems AmigaNet";
    default:            return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* mmap-safe region tester                                              */
/* ------------------------------------------------------------------ */

static int try_map(int fd, unsigned long offset, unsigned char *buf, int len)
{
    volatile unsigned char *mem;
    int i;

    if (setjmp(env) != 0)
    {
        fprintf(stderr, "      BUS ERROR at 0x%08lX\n", offset);
        return -1;
    }

    mem = (unsigned char *)mmap((caddr_t)0, len, PROT_READ, MAP_SHARED,
                                fd, (off_t)offset);
    if (mem == (unsigned char *)-1)
    {
        fprintf(stderr, "      mmap(0x%08lX) failed\n", offset);
        return -1;
    }

    for (i = 0; i < len; i++)
        buf[i] = mem[i];

    munmap((caddr_t)mem, len);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Hardware probe                                                      */
/* ------------------------------------------------------------------ */

static void scan_slots(int fd, const char *label, unsigned long base, unsigned long end, unsigned long step, int check_mac)
{
    unsigned long addr;
    for (addr = base; addr <= end; addr += step)
    {
        unsigned char ac_raw[AC_MAP_SIZE];
        int i, all_zero, all_ff;
        int slot = (int)((addr - base) / step);

        printf("  Slot %d at 0x%08lX: ", slot, addr);

        if (try_map(fd, addr, ac_raw, AC_MAP_SIZE) != 0)
        {
            printf("mmap failed\n");
            continue;
        }

        all_zero = all_ff = 1;
        for (i = 0; i < AC_MAP_SIZE; i++)
        {
            if (ac_raw[i] != 0) all_zero = 0;
            if (ac_raw[i] != 0xFF) all_ff = 0;
        }
        if (all_zero)
        {
            printf("all zeros\n");
            continue;
        }
        if (all_ff)
        {
            printf("all 0xFF\n");
            continue;
        }

        printf("present\n");

        /* Try AutoConfig decode */
        if (is_autoconfig(ac_raw))
        {
            unsigned char er_type = ac_byte(ac_raw, AC_TYPE);
            unsigned char prod    = ac_byte(ac_raw, AC_PRODUCT);
            unsigned short manuf  = ac_word(ac_raw, AC_MANUF);
            unsigned long serial  = ac_long(ac_raw, AC_SERIAL);
            unsigned short diag   = ac_word(ac_raw, AC_DIAGVEC);
            unsigned long size    = size_table[er_type & ERT_SIZEMASK];
            const char *name      = identify_board(manuf, prod);

            printf("    AutoConfig: manuf=%04X prod=%02X serial=%08lX\n",
                   (unsigned)manuf, (unsigned)prod, serial);
            printf("    Size: %s  DiagVec: %04X\n", size_str(size),
                   (unsigned)diag);
            if (name)
                printf("    => %s\n", name);
            else
                printf("    => Unknown board\n");
        }
        else
        {
            printf("    Raw bytes at +0x00:");
            for (i = 0; i < 16; i++)
                printf(" %02x", ac_raw[i]);
            printf("\n");
        }

        if (check_mac)
        {
            unsigned char mac[12];
            printf("    +0xffc0 (MAC PROM): ");
            if (try_map(fd, addr + NE8390_ADDRPROM_OFFSET, mac, 12) == 0)
            {
                int valid = 1;
                for (i = 0; i < 6; i++)
                    if (mac[i*2] != 0xFF) break;
                if (i == 6) valid = 0;
                for (i = 0; i < 6; i++)
                    if (mac[i*2] != 0x00) break;
                if (i == 6) valid = 0;
                for (i = 1; i < 6; i++)
                    if (mac[i*2] != mac[0]) break;
                if (i == 6) valid = 0;
                for (i = 0; i < 6; i++)
                    printf("%02x%c", mac[i*2], i<5?':':'\n');
                if (valid)
                    printf("    => NE2000 MAC\n");
                else
                    printf("    (no MAC data)\n");
            }

            printf("    +0xffe1 (NE2000 NIC): ");
            {
                unsigned char nic[4];
                if (try_map(fd, addr + 0xffe1, nic, 2) == 0)
                {
                    printf("regs=%02x %02x",
                           (unsigned)nic[0], (unsigned)nic[1]);
                    if (nic[0] == 0x21 || nic[1] == 0x21)
                        printf(" DP8390 signature?\n");
                    else
                        printf("\n");
                }
            }
        }

        printf("\n");
    }
}

static void probe_hardware(void)
{
    int fd;
    const char *devpath = NULL;

    fd = open("/dev/mem", O_RDONLY);
    if (fd >= 0) {
        devpath = "/dev/mem";
    } else {
        fd = open("/dev/pmem", O_RDONLY);
        if (fd >= 0) devpath = "/dev/pmem";
    }

    if (!devpath)
    {
        printf("  (neither /dev/mem nor /dev/pmem accessible)\n");
        return;
    }
    printf("  Using %s\n\n", devpath);

    /* Zorro II I/O: scan both 0xE80000 (kernel method 2) and 0xE90000 (lszorro) */
    printf("  --- Zorro II I/O: 0xE80000 range (kernel method 2) ---\n");
    scan_slots(fd, "0xE80000", 0x00E80000UL, 0x00E8FFFFUL, 0x00010000UL, 1);

    printf("  --- Zorro II I/O: 0xE90000 range (lszorro) ---\n");
    scan_slots(fd, "0xE90000", 0x00E90000UL, 0x00EFFFFFUL, 0x00010000UL, 1);

    /* Zorro II memory: look for AutoConfig-enabled boards */
    printf("  --- Zorro II memory: 0x200000-0x9FFFFF ---\n");
    scan_slots(fd, "0x200000", 0x00200000UL, 0x009FFFFFUL, 0x00010000UL, 0);

    /* Also try probing /dev/bootinfo (if it exists) */
    printf("  --- /dev/bootinfo (if available) ---\n");
    {
        int bfd = open("/dev/bootinfo", O_RDONLY);
        if (bfd >= 0)
        {
            printf("  /dev/bootinfo opened successfully\n");
            close(bfd);
        }
        else
            printf("  /dev/bootinfo: %s\n", strerror(errno));
    }

    /* /dev/bootinfo probe */
    printf("  --- /dev/bootinfo (if available) ---\n");
    {
        int bfd = open("/dev/bootinfo", O_RDONLY);
        if (bfd >= 0)
        {
            printf("  /dev/bootinfo opened successfully\n");
            close(bfd);
        }
        else
            printf("  /dev/bootinfo: %s\n", strerror(errno));
    }

    printf("\n  (end of hardware probe)\n");
    close(fd);
}

/* ------------------------------------------------------------------ */
/* STREAMS driver probe                                                 */
/* ------------------------------------------------------------------ */

static void
probe_driver(const char *dev, const char *name)
{
    int fd = open(dev, O_RDWR);
    if (fd >= 0)
    {
        printf("  %s (%s): DETECTED (open OK)\n", dev, name);
        close(fd);
    }
    else
    {
        if (errno == ENODEV || errno == ENXIO)
            printf("  %s (%s): no board\n", dev, name);
        else
            printf("  %s (%s): %s\n", dev, name, strerror(errno));
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int
main()
{
    int i;

    signal(SIGBUS,  bus_handler);
    signal(SIGSEGV, bus_handler);

    printf("\nZorro II Expansion Device Probe\n");
    printf("================================\n\n");

    /* Part 1: Direct hardware access via mmap on /dev/mem */
    printf("1. Direct hardware probe (/dev/mem):\n");
    probe_hardware();

    /* Part 2: STREAMS driver probe */
    printf("\n2. STREAMS driver probe:\n");

    for (i = 0; i < 4; i++)
    {
        char path[32];
        sprintf(path, "/dev/hya%d", i);
        probe_driver(path, "Hydra/NE8390");
    }

    for (i = 0; i < 4; i++)
    {
        char path[32];
        sprintf(path, "/dev/aen%d", i);
        probe_driver(path, "A2065/LANCE");
    }

    printf("\nDone.\n");
    return 0;
}
