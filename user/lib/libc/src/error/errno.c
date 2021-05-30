#include <errno.h>

#ifdef LIBC_NOTLS
int errno = 0;
#else
_Thread_local int errno = 0;
#endif
