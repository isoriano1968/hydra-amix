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
#define ID_HYDRA_0849 0x08490001UL  /* Hydra Systems AmigaNet */
#define ID_HYDRA_041D 0x041D0001UL  /* Ameristar (possible rebadge) */

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
    case ID_HYDRA_0849: return "Hydra Systems AmigaNet";
    case ID_HYDRA_041D: return "Ameristar A2065 (rebadge?)";
    default:            return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* mmap-safe region tester                                              */
/* ------------------------------------------------------------------ */

static int
try_map(int fd, unsigned long offset, unsigned char *buf, int len)
{
    volatile unsigned char *mem;
    int i;

    if (setjmp(env) != 0)
    {
        fprintf(stderr, "    BUS ERROR at 0x%08lX\n", offset);
        return -1;
    }

    mem = (unsigned char *)mmap((caddr_t)0, len, PROT_READ, MAP_SHARED,
                                fd, (off_t)offset);
    if (mem == (unsigned char *)-1)
        return -1;

    /* Read bytes into buffer (may trigger SIGBUS) */
    for (i = 0; i < len; i++)
        buf[i] = mem[i];

    munmap((caddr_t)mem, len);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Hardware probe                                                      */
/* ------------------------------------------------------------------ */

static void
probe_hardware(void)
{
    int fd, found;

    fd = open("/dev/mem", O_RDONLY);
    if (fd < 0)
    {
        fd = open("/dev/pmem", O_RDONLY);
        if (fd < 0)
        {
            printf("  (no /dev/mem access)\n");
            return;
        }
    }

    found = 0;

    /* Scan Zorro II I/O slots */
    printf("\n  --- Zorro II I/O slots (0xE90000-0xEFFFFF) ---\n");
    {
        unsigned long addr;
        for (addr = ZII_IO_BASE; addr <= ZII_IO_END; addr += ZII_IO_STEP)
        {
            unsigned char ac_raw[AC_MAP_SIZE];
            int i, all_zero, all_ff;
            int slot = (int)((addr - ZII_IO_BASE) / ZII_IO_STEP);

            if (try_map(fd, addr, ac_raw, AC_MAP_SIZE) != 0)
                continue;

            all_zero = all_ff = 1;
            for (i = 0; i < AC_MAP_SIZE; i++)
            {
                if (ac_raw[i] != 0) all_zero = 0;
                if (ac_raw[i] != 0xFF) all_ff = 0;
            }
            if (all_zero || all_ff)
                continue;

            found = 1;
            printf("  Slot %d (0x%08lX):\n", slot, addr);

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

            /* Try NE2000 MAC PROM at +0xffc0 */
            {
                unsigned char mac[12];
                if (try_map(fd, addr + NE8390_ADDRPROM_OFFSET, mac, 12) == 0)
                {
                    int valid = 1;
                    for (i = 0; i < 6; i++)
                        if (mac[i*2] == 0 || mac[i*2] == 0xFF)
                            valid = 0;
                    if (valid)
                    {
                        printf("    MAC PROM: %02x:%02x:%02x:%02x:%02x:%02x\n",
                               mac[0], mac[2], mac[4], mac[6], mac[8], mac[10]);
                        printf("    => NE2000 card detected\n");
                    }
                    else
                    {
                        printf("    +0xffc0:");
                        for (i = 0; i < 6; i++)
                            printf(" %02x", mac[i*2]);
                        printf(" (no valid MAC)\n");
                    }
                }
            }

            printf("\n");
        }
    }

    /* Scan Zorro II memory slots */
    printf("  --- Zorro II memory slots (0x200000-0x9FFFFF) ---\n");
    {
        unsigned long addr;
        for (addr = ZII_MEM_BASE; addr <= ZII_MEM_END; addr += ZII_MEM_STEP)
        {
            unsigned char ac_raw[AC_MAP_SIZE];
            int i, all_zero, all_ff;

            if (try_map(fd, addr, ac_raw, AC_MAP_SIZE) != 0)
                continue;

            all_zero = all_ff = 1;
            for (i = 0; i < AC_MAP_SIZE; i++)
            {
                if (ac_raw[i] != 0) all_zero = 0;
                if (ac_raw[i] != 0xFF) all_ff = 0;
            }
            if (all_zero || all_ff)
                continue;

            if (!is_autoconfig(ac_raw))
                continue;

            found = 1;
            printf("  0x%08lX:\n", addr);

            {
                unsigned char er_type = ac_byte(ac_raw, AC_TYPE);
                unsigned char prod    = ac_byte(ac_raw, AC_PRODUCT);
                unsigned short manuf  = ac_word(ac_raw, AC_MANUF);
                unsigned long serial  = ac_long(ac_raw, AC_SERIAL);
                unsigned long size    = size_table[er_type & ERT_SIZEMASK];
                const char *name      = identify_board(manuf, prod);

                printf("    AutoConfig: manuf=%04X prod=%02X serial=%08lX\n",
                       (unsigned)manuf, (unsigned)prod, serial);
                printf("    Size: %s\n", size_str(size));
                if (name)
                    printf("    => %s\n", name);
            }
            printf("\n");
        }
    }

    if (!found)
        printf("  (no devices detected via /dev/mem)\n");

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
