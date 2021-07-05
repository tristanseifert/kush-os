#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stdint.h>
#include <stddef.h>
#include <_libc.h>

// TODO: expand all declarations to have nullability specifiers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

#ifdef __cplusplus
extern "C" {
#endif

double	 atof(const char *);
int	 atoi(const char *);
long	 atol(const char *);


typedef struct {
    /// quotient of division
    int	quot;
    /// remainder of division
    int	rem;
} div_t;
div_t	 div(int, int);
int	 abs(int);

typedef struct {
    long	quot;
    long	rem;
} ldiv_t;
ldiv_t	 ldiv(long, long);
long	 labs(long);

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;
lldiv_t	 lldiv(long long, long long);


/* LONGLONG */
long long
     atoll(const char *);
/* LONGLONG */
long long
     llabs(long long);
/* LONGLONG */
/* LONGLONG */
long long
     strtoll(const char * __restrict, char ** __restrict, int);
/* LONGLONG */
unsigned long long
     strtoull(const char * __restrict, char ** __restrict, int);

double	 strtod(const char * __restrict, char ** __restrict);
float	 strtof(const char * __restrict, char ** __restrict);
long	 strtol(const char * __restrict, char ** __restrict, int);
long double
	 strtold(const char * __restrict, char ** __restrict);
unsigned long
	 strtoul(const char * __restrict, char ** __restrict, int);

int	 mblen(const char *, size_t);
size_t	 mbstowcs(wchar_t * __restrict , const char * __restrict, size_t);
int	 mbtowc(wchar_t * __restrict, const char * __restrict, size_t);
int	 wctomb(char *, wchar_t);
size_t	 wcstombs(char * __restrict, const wchar_t * __restrict, size_t);


// crappy C standard random; provided for compatibility only
int	 rand(void);
void	 srand(unsigned);

/// Terminates the current program with the specified exit code
_Noreturn void	 exit(int);
_Noreturn void	 _Exit(int);
/// Run the given function at program exit time
int	 atexit(void (* _Nonnull)(void));

LIBC_EXPORT char *getenv(const char *);
LIBC_EXPORT int setenv(const char *name, const char *value, int overwrite);
LIBC_EXPORT int putenv(char *string);
LIBC_EXPORT int unsetenv(const char *name);

LIBC_EXPORT int heapsort(void *base, size_t nel, size_t width,
        int (*compare)(const void *, const void *));
LIBC_EXPORT int mergesort(void *base, size_t nel, size_t width,
        int (*compare)(const void *, const void *));
LIBC_EXPORT void	 qsort(void *, size_t, size_t,
	    int (* _Nonnull)(const void *, const void *));
LIBC_EXPORT void	*bsearch(const void *, const void *, size_t,
	    size_t, int (*)(const void * _Nonnull, const void *));

/// executes a system command; not currently implemented
int	 system(const char *);


/// Aborts program execution
_Noreturn LIBC_EXPORT void abort();

/// Allocates zeroed memory
LIBC_EXPORT void *calloc(const size_t count, const size_t size);
/// Allocates memory
LIBC_EXPORT void *malloc(const size_t numBytes);
/// Frees previously allocated memory
LIBC_EXPORT void free(void *ptr);
/// Resizes the previously made allocation.
LIBC_EXPORT void *realloc(void *ptr, const size_t newSize);
/// Resizes a previously made allocation, iff it can be done in-place. Returns NULL if not.
LIBC_EXPORT void *realloc_in_place(void *ptr, const size_t newSize);
/// Aligned malloc
LIBC_EXPORT void *dlmemalign(const size_t alignment, const size_t size);
/// Aligned malloc
LIBC_EXPORT int posix_memalign(void **outPtr, const size_t alignment, const size_t size);

/// Max value returned by random: a 32-bit quantity
#define RAND_MAX 0xFFFFFFFF
/// Returns a random long value.
LIBC_EXPORT long random();

/// Returns a random 32-bit value
LIBC_EXPORT uint32_t arc4random(void);
/// Fills the buffer with random data
LIBC_EXPORT void arc4random_buf(void *buf, size_t nBytes);
/// Generates an uniformly distributed random number between 0 and bound
LIBC_EXPORT uint32_t arc4random_uniform(uint32_t upperBound);

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop

#endif
