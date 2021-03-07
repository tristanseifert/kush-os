#ifndef _LIBC_FENV_H
#define _LIBC_FENV_H

#include <stdint.h>

#if defined(__i386__) || defined(__amd64__)
#include <fenv_x86.h>
#else
#error Add support to fpenv.h for this architecture
#endif

#endif
