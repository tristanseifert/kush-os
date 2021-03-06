#ifndef LIBSYSTEM_ENDIAN_H
#define LIBSYSTEM_ENDIAN_H

// include the endian file for the current arch
#if defined(__i386__)
#include <sys/x86/endian.h>
#elif defined(__amd64__)
#include <sys/amd64/endian.h>
#else
#error Add architecture to libsystem endian.h
#endif

#endif
