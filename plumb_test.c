#include <stdio.h>
#include <fcntl.h>
#include <stropts.h>
#include <errno.h>

int main()
{
    int fd;
    int r;

    fd = open("/dev/hya0", O_RDWR);
    if (fd < 0) { printf("open failed errno=%d\n", errno); return 1; }
    printf("Opened fd=%d\n", fd);

    r = ioctl(fd, I_PUSH, "sockmod");
    printf("I_PUSH sockmod: r=%d errno=%d\n", r, errno);

    if (r == 0) {
	r = ioctl(fd, I_PUSH, "ip");
	printf("I_PUSH ip: r=%d errno=%d\n", r, errno);
    }

    close(fd);
    return 0;
}
