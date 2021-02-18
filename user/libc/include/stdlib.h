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

char	*getenv(const char *);

void	 qsort(void *, size_t, size_t,
	    int (* _Nonnull)(const void *, const void *));
void	*bsearch(const void *, const void *, size_t,
	    size_t, int (*)(const void * _Nonnull, const void *));

/// executes a system command; not currently implemented
int	 system(const char *);


/// Aborts program execution
LIBC_EXPORT _Noreturn void abort();

/// Allocates zeroed memory
LIBC_EXPORT void *calloc(const size_t count, const size_t size);
/// Allocates memory
LIBC_EXPORT void *malloc(const size_t numBytes);
/// Frees previously allocated memory
LIBC_EXPORT void free(void *ptr);
/// Resizes the previously made allocation.
LIBC_EXPORT void *realloc(void *ptr, const size_t size);
/// Aligned malloc
LIBC_EXPORT int posix_memalign(void **, size_t, size_t);

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop

#endif
