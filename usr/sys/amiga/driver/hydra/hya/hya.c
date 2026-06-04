/*
** hya - Hydra Systems AmigaNet control utility
**
** -S		Silent check (exit 0 if board found, 1 if not)
** -c		Get configuration (MAC, board base)
** -n		Print number of boards
** -d devname	Set device name (default /dev/hya0)
*/

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "hydrauser.h"

static char *program_name, device_name[256];

int
main(int argc, char **argv)
{
    int c, error_flag = 0;
    int number_of_boards_p = 0, get_config_p = 0, silent_p = 0;
    int fd, count;
    hydra_config_t cfg;

    program_name = *argv;

    while ((c = getopt(argc, argv, "Sncd:?")) != EOF)
    {
	switch (c)
	{
	case 'S':
	    silent_p++;
	    number_of_boards_p++;
	    break;

	case 'n':
	    number_of_boards_p++;
	    break;

	case 'c':
	    get_config_p++;
	    break;

	case 'd':
	    if (optarg[0] == '/')
		strcpy(device_name, optarg);
	    else
		sprintf(device_name, "/dev/%s", optarg);
	    break;

	case '?':
	    error_flag++;
	    break;
	}

	if (error_flag)
	{
	    fprintf(stderr, "usage: %s [-Snc?] [-d dev]\n", program_name);
	    return 1;
	}
    }

    if (device_name[0] == '\0')
	strcpy(device_name, "/dev/hya0");

    fd = open(device_name, O_RDONLY);
    if (fd < 0)
    {
	if (!silent_p)
	    fprintf(stderr, "%s: couldn't open %s (%s)\n",
		    program_name, device_name, strerror(errno));

	if (number_of_boards_p)
	{
	    if (!silent_p)
		printf("0\n");
	    return 1;
	}

	return 1;
    }

    if (number_of_boards_p)
    {
	if (ioctl(fd, HYDRA_NUMBER_OF_BOARDS, &count) < 0)
	    count = 0;

	if (!silent_p)
	    printf("%d\n", count);

	close(fd);
	return count == 0;
    }

    if (ioctl(fd, HYDRA_GET_CONFIG, &cfg) < 0)
    {
	fprintf(stderr, "%s: couldn't get config from %s (%s)\n",
		program_name, device_name, strerror(errno));
	close(fd);
	return 1;
    }

    printf("Device:\t\t%s\n", device_name);

    printf("Ethernet:\t%02x:%02x:%02x:%02x:%02x:%02x\n",
	   cfg.paddress[0], cfg.paddress[1], cfg.paddress[2],
	   cfg.paddress[3], cfg.paddress[4], cfg.paddress[5]);

    if (get_config_p)
    {
	printf("Board Base:\t0x%lx\n", cfg.board_base);
	printf("Board Debug:\t0x%x\n", cfg.board_debug);
    }

    close(fd);
    return 0;
}
