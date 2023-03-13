#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

static inline long elapse(struct timespec start, struct timespec end)
{
    return ((long) 1.0e+9 * end.tv_sec + end.tv_nsec) -
           ((long) 1.0e+9 * start.tv_sec + start.tv_nsec);
}

int main()
{
    struct timespec t1, t2;
    char buf[1];
    int offset = 93; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);

        clock_gettime(CLOCK_REALTIME, &t1);
        long kernel_time = write(fd, buf, 1);
        clock_gettime(CLOCK_REALTIME, &t2);
        printf("%d ", i);
        /* user space execute time */
        printf("%ld ", elapse(t1, t2));
        /* kernel space execute time */
        printf("%ld ", kernel_time);
        /* transfer time between kernel and user space */
        printf("%ld\n", elapse(t1, t2) - kernel_time);
    }

    close(fd);
    return 0;
}
