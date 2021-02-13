#ifndef LIBC_INTERNAL_H
#define LIBC_INTERNAL_H

/*
 * Visibility macro for functions
 */
#ifdef BUILDING_LIBC
#define LIBC_EXPORT __attribute__((__visibility__("default")))
#define LIBC_INTERNAL __attribute__((__visibility__("hidden")))
#else
#define LIBC_EXPORT extern
#endif

#endif
