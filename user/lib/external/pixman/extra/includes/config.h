/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Whether we have alarm() */
/* #undef HAVE_ALARM */

/* Whether the compiler supports __builtin_clz */
#define HAVE_BUILTIN_CLZ /**/

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Whether we have FE_DIVBYZERO */
#define HAVE_FEDIVBYZERO 1

/* Whether we have feenableexcept() */
#define HAVE_FEENABLEEXCEPT 1

/* Define to 1 if we have <fenv.h> */
#define HAVE_FENV_H 1

/* Whether the tool chain supports __float128 */
/* #undef HAVE_FLOAT128 */

/* Whether the compiler supports GCC vector extensions */
#define HAVE_GCC_VECTOR_EXTENSIONS /**/

/* Define to 1 if you have the `getisax' function. */
/* #undef HAVE_GETISAX */

/* Whether we have getpagesize() */
/* #undef HAVE_GETPAGESIZE */

/* Whether we have gettimeofday() */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `pixman-1' library (-lpixman-1). */
/* #undef HAVE_LIBPIXMAN_1 */

/* Whether we have libpng */
#define HAVE_LIBPNG 1

/* Whether we have mmap() */
/* #undef HAVE_MMAP */

/* Whether we have mprotect() */
/* #undef HAVE_MPROTECT */

/* Whether we have posix_memalign() */
#define HAVE_POSIX_MEMALIGN 1

/* Whether pthreads is supported */
/* #undef HAVE_PTHREADS */

/* Whether we have sigaction() */
/* #undef HAVE_SIGACTION */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if we have <sys/mman.h> */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "pixman"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "pixman@lists.freedesktop.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "pixman"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "pixman 0.40.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "pixman"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.40.0"

/* enable output that can be piped to gnuplot */
/* #undef PIXMAN_GNUPLOT */

/* enable TIMER_BEGIN/TIMER_END macros */
/* #undef PIXMAN_TIMERS */

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* The compiler supported TLS storage class */
#define TLS __thread

/*
 * TODO: figure out why this, when enabled, produces broken shared libraries.
 *
 * It likely has to do with the fact that ldyldo doesn't run the initializers in the "correct" order
 * as defined by the POSIX standards.
 */
/* Whether the tool chain supports __attribute__((constructor)) */
// #define TOOLCHAIN_SUPPORTS_ATTRIBUTE_CONSTRUCTOR /**/

/* use ARM IWMMXT compiler intrinsics */
/* #undef USE_ARM_IWMMXT */

/* use ARM NEON assembly optimizations */
/* #undef USE_ARM_NEON */

/* use ARM SIMD assembly optimizations */
/* #undef USE_ARM_SIMD */

/* use GNU-style inline assembler */
#define USE_GCC_INLINE_ASM 1

/* use Loongson Multimedia Instructions */
/* #undef USE_LOONGSON_MMI */

/* use MIPS DSPr2 assembly optimizations */
/* #undef USE_MIPS_DSPR2 */

/* use OpenMP in the test suite */
/* #undef USE_OPENMP */

/* use SSE2 compiler intrinsics */
#define USE_SSE2 1

/* use SSSE3 compiler intrinsics */
#define USE_SSSE3 1

/* use VMX compiler intrinsics */
/* #undef USE_VMX */

/* use x86 MMX compiler intrinsics */
#define USE_X86_MMX 1

/* Version number of package */
#define VERSION "0.40.0"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif
