#ifndef _SYS_TIME_H_
#define	_SYS_TIME_H_

#include <time.h>

#ifndef _SUSECONDS_T_DECLARED
typedef    int    suseconds_t;
#define    _SUSECONDS_T_DECLARED
#endif

struct timeval {
    time_t      tv_sec;     /* seconds */
    suseconds_t tv_usec;    /* microseconds */
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#endif
