#ifndef LIBSYSTEM_INTERNAL_H
#define LIBSYSTEM_INTERNAL_H

/*
 * Visibility macro for functions
 */
#ifdef BUILDING_LIBSYSTEM
#define LIBSYSTEM_EXPORT __attribute__((__visibility__("default")))
#define LIBSYSTEM_INTERNAL __attribute__((__visibility__("hidden")))
#else
#define LIBSYSTEM_EXPORT extern
#endif

#endif
