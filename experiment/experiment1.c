#include <fcntl.h>
#include <math.h>
#include <stdint.h>
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

static long long double_fib(int n)
{
    /* find first 1 */
    int h = 31 - __builtin_clz(n);

    long long a = 0;
    long long b = 1;
    long long mask;
    for (int i = h; i >= 0; --i) {
        long long c = a * (2 * b - a);
        long long d = a * a + b * b;

        mask = -!!(n & (1UL << i));
        a = (c & ~mask) + (d & mask);
        b = (c & mask) + d;
    }
    return a;
}

static long long double_fib1(int n)
{
    long long a = 0;
    long long b = 1;
    for (int i = 31; i >= 0; --i) {
        long long c = a * (2 * b - a);
        long long d = a * a + b * b;

        long long mask = -!!(n & (1UL << i));
        a = (c & ~mask) + (d & mask);
        b = (c & mask) + d;
    }
    return a;
}

int main()
{
    struct timespec t1, t2;
    int offset = 93; /* TODO: try test something bigger than the limit */

    /*
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    */

    for (int i = 0; i <= offset; i++) {
        clock_gettime(CLOCK_REALTIME, &t1);
        double_fib1(i);
        clock_gettime(CLOCK_REALTIME, &t2);
        printf("%d ", i);
        printf("%ld ", elapse(t1, t2));

        clock_gettime(CLOCK_REALTIME, &t1);
        double_fib(i);
        clock_gettime(CLOCK_REALTIME, &t2);
        printf("%ld\n", elapse(t1, t2));
    }

    return 0;
}
