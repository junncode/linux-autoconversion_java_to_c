#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t);
void ssu_convert(int argc, char *argv[]);

int main (int argc, char *argv[])
{
    struct timeval begin_t, end_t;
    gettimeofday(&begin_t, NULL);

    ssu_convert(argc, argv);

    gettimeofday(&end_t, NULL);
    ssu_runtime(&begin_t, &end_t);

    exit(0);
}

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t)
{
    end_t->tv_sec -= begin_t->tv_sec;

    if (end_t->tv_usec < begin_t->tv_usec) {
        end_t->tv_sec--;
        end_t->tv_usec += 1000000;
    }

    end_t->tv_usec -= begin_t->tv_usec;
    printf("Runtime: %ld:%06ld(sec:usec)\n", end_t->tv_sec, end_t->tv_usec);
}
