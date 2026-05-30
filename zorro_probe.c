/*
 * Zorro II Expansion Device Probe
 * Detects expansion boards by direct hardware access (if available)
 * and by probing the kernel's hydra and aen STREAMS drivers.
 *
 * Compile:  gcc -O -o zorro_probe zorro_probe.c
 * Run:      ./zorro_probe
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

static jmp_buf env;

static void
bus_handler(int sig)
{
    longjmp(env, 1);
}

static int
try_read(int fd, unsigned long off, unsigned char *buf, int len)
{
    int i;

    if (setjmp(env) != 0)
    {
	for (i = 0; i < len; i++)
	    buf[i] = 0xFF;
	return -1;
    }
    if (lseek(fd, off, SEEK_SET) == -1)
	return -1;
    if (read(fd, buf, len) != len)
	return -1;
    return 0;
}

static void
probe_hardware(void)
{
    int fd, slot, found;

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
    for (slot = 0; slot < 8; slot++)
    {
	unsigned long base = 0x00E80000UL + (unsigned long)slot * 0x10000UL;
	unsigned char buf[64], mac[6], lance[4];
	int i, all_zero, all_ff;

	if (try_read(fd, base, buf, 64) != 0)
	    continue;

	all_zero = all_ff = 1;
	for (i = 0; i < 64; i++)
	{
	    if (buf[i] != 0) all_zero = 0;
	    if (buf[i] != 0xFF) all_ff = 0;
	}

	if (all_zero)
	{
	    printf("  Slot %d (0x%08lX): all zeros (unmapped?)\n", slot, base);
	    continue;
	}
	if (all_ff)
	{
	    printf("  Slot %d (0x%08lX): empty\n", slot, base);
	    continue;
	}

	found = 1;
	printf("  Slot %d (0x%08lX):\n", slot, base);
	printf("    +0x00:");
	for (i = 0; i < 32; i++)
	    printf(" %02x", buf[i]);
	printf("\n");

	try_read(fd, base + 0xffc0UL, mac, 12);
	printf("    +0xffc0:");
	for (i = 0; i < 6; i++)
	    printf(" %02x", mac[i*2]);
	printf("\n");

	try_read(fd, base, lance, 4);
	{
	    unsigned short csr0 = (unsigned short)(lance[0]<<8 | lance[1]);
	    if (csr0 == 0x0004)
		printf("    => LANCE CSR0=STOP (A2065?)\n");
	    else if (csr0 == 0x0000)
		printf("    => LANCE CSR0=0 (A2065?)\n");
	    else
		printf("    => Unknown\n");
	}
    }

    if (!found)
	printf("  (no devices detected via /dev/mem)\n");

    close(fd);
}

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
	if (errno == ENODEV)
	    printf("  %s (%s): no board\n", dev, name);
	else if (errno == ENXIO)
	    printf("  %s (%s): no board\n", dev, name);
	else
	    printf("  %s (%s): %s\n", dev, name, strerror(errno));
    }
}

int
main()
{
    int i;

    signal(SIGBUS, bus_handler);
    signal(SIGSEGV, bus_handler);

    printf("\nZorro II Expansion Device Probe\n");
    printf("================================\n\n");

    /* Part 1: Direct hardware access (limited on Amix) */
    printf("1. Direct hardware probe (/dev/mem):\n");
    probe_hardware();

    /* Part 2: STREAMS driver probe (opens /dev/hya* and /dev/aen*) */
    printf("\n2. STREAMS driver probe:\n");

    /* Try all 4 hydra minor devices */
    for (i = 0; i < 4; i++)
    {
	char path[32];
	sprintf(path, "/dev/hya%d", i);
	probe_driver(path, "Hydra/NE8390");
    }

    /* Try A2065 driver */
    for (i = 0; i < 4; i++)
    {
	char path[32];
	sprintf(path, "/dev/aen%d", i);
	probe_driver(path, "A2065/LANCE");
    }

    printf("\nDone.\n");
    return 0;
}
