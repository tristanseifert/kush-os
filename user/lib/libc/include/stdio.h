#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <_libc.h>

// constants for seeking
#ifndef SEEK_SET
#define	SEEK_SET                        0
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR                        1
#endif
#ifndef SEEK_END
#define	SEEK_END                        2
#endif

// maximum length of filenames
#define	FILENAME_MAX                    1024
// min size of buffered IO Buffers
#define BUFSIZ                          (1024)

// declare some stuff to the actual symbol names
#define	stdin	__stdinp
#define	stdout	__stdoutp
#define	stderr	__stderrp

#define	EOF	(-1)

// TODO: expand all declarations to have nullability specifiers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

#ifdef __cplusplus
extern "C" {
#endif

typedef int fpos_t;
typedef struct __libc_file_stream FILE;

#ifndef _STDSTREAM_DECLARED
extern FILE *__stdinp;
extern FILE *__stdoutp;
extern FILE *__stderrp;
#define	_STDSTREAM_DECLARED
#endif

void clearerr(FILE *);
int	 feof(FILE *);
int	 ferror(FILE *);
int     fileno(FILE *stream);

int	 fclose(FILE *);
int	 fflush(FILE *);
int	 fgetc(FILE *);
int	 fgetpos(FILE * __restrict, fpos_t * __restrict);
char	*fgets(char * __restrict, int, FILE * __restrict);
FILE	*fopen(const char * __restrict, const char * __restrict);
FILE    *fdopen(int fd, const char *mode);

int	 fprintf(FILE * __restrict, const char * __restrict, ...);
int	 fputc(int, FILE *);
int	 fputs(const char * __restrict, FILE * __restrict);
size_t	 fread(void * __restrict, size_t, size_t, FILE * __restrict);
FILE	*freopen(const char * __restrict, const char * __restrict, FILE * __restrict);
int	 fscanf(FILE * __restrict, const char * __restrict, ...);
int	 fseek(FILE *, long, int);
int     fseeko(FILE *stream, off_t offset, int whence);
int	 fsetpos(FILE *, const fpos_t *);
long	 ftell(FILE *);
off_t   ftello(FILE *stream);
size_t	 fwrite(const void * __restrict, size_t, size_t, FILE * __restrict);
int	 getc(FILE *);
int	 getchar(void);
void	 perror(const char *);
int	 printf(const char * __restrict, ...);
int	 putc(int, FILE *);
int	 putchar(int);
int	 puts(const char *);
int	 remove(const char *);
int	 rename(const char *, const char *);
void	 rewind(FILE *);
int	 scanf(const char * __restrict, ...);
void	 setbuf(FILE * __restrict, char * __restrict);
int	 setvbuf(FILE * __restrict, char * __restrict, int, size_t);
int	 sprintf(char * __restrict, const char * __restrict, ...);
int	 sscanf(const char * __restrict, const char * __restrict, ...);
FILE	*tmpfile(void);
char	*tmpnam(char *);
int	 ungetc(int, FILE *);
int	 vfprintf(FILE * __restrict, const char * __restrict, va_list);
int	 vprintf(const char * __restrict, va_list);
int	 vsprintf(char * __restrict, const char * __restrict, va_list);
int	 snprintf(char * __restrict, size_t, const char * __restrict,
	    ...); // __printflike(3, 4);
int	 vsnprintf(char * __restrict, size_t, const char * __restrict, va_list); //__printflike(3, 0);
int	 vfscanf(FILE * __restrict, const char * __restrict, va_list);
	    // __scanflike(2, 0);
int	 vscanf(const char * __restrict, va_list); // __scanflike(1, 0);
int	 vsscanf(const char * __restrict, const char * __restrict, va_list); // __scanflike(2, 0);

LIBC_EXPORT int asprintf(char **strp, const char *fmt, ...);
LIBC_EXPORT int vasprintf(char **strp, const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop

#endif
