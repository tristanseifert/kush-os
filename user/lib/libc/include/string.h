#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memclr(void *start, const size_t count);

void	*memchr(const void *, int, size_t) ;
int	 memcmp(const void *, const void *, size_t) ;
void	*memcpy(void * __restrict, const void * __restrict, size_t);
void	*memmove(void *, const void *, size_t);
void	*memset(void *, int, size_t);
char	*strcat(char * __restrict, const char * __restrict);
char	*strchr(const char *, int) ;
int	 strcmp(const char *, const char *) ;
int	 strcoll(const char *, const char *);
char	*strcpy(char * __restrict, const char * __restrict);
size_t	 strcspn(const char *, const char *) ;
char	*strerror(int);
int strerror_r(int errnum, char *buf, size_t buflen);
size_t	 strlen(const char *);
char	*strncat(char * __restrict, const char * __restrict, size_t);
int	 strncmp(const char *, const char *, size_t) ;
char	*strncpy(char * __restrict, const char * __restrict, size_t);
char	*strpbrk(const char *, const char *) ;
char	*strrchr(const char *, int) ;
size_t	 strspn(const char *, const char *) ;
char	*strstr(const char *, const char *) ;
char	*strtok(char * __restrict, const char * __restrict);
size_t	 strxfrm(char * __restrict, const char * __restrict, size_t);


#ifdef __cplusplus
}
#endif

#endif
