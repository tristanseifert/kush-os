#ifndef LIBDRIVER_BASE85_H
#define	LIBDRIVER_BASE85_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup base85 Base85 Converter.
  * Base85 RFC 1924 version. The character set is, in order, 0–9, A–Z, a–z, and
  * then the 23 characters !#$%&()*+-;<=>?@^_`{|}~.
  * @{ */

/** Convert a binary memory block in a base85 null-terminated string.
  * If the size of the source memory block is not a multiple of four,
  * as many zeros as necessary are added to convert it to a multiple of four.
  * @param dest Destination memory where to put the base85 null-terminated string.
  * @param src Source binary memory block.
  * @param size Size in bytes of source binary memory block.
  * @return A pointer to the null character of the base85 null-terminated string. */
char *bintob85(char* restrict dest, void const* restrict src, size_t size);

/** Convert a base85 string to binary format.
  * @param dest Destination memory block.
  * @param src Source base85 string.
  * @return If success a pointer to the next byte in memory block.
  *         Null if string has a bad format.  */
void *b85tobin(void* restrict dest, char const* restrict src);

/** @ } */

#ifdef __cplusplus
}
#endif

#endif
