/**
 * Implementations of functions typically found in `string.h`
 *
 * Unless specified otherwise, these routines were lifted from OpenBSD libc.
 */
#include "Runtime/String.h"

/**
 * Fill a region of memory with a byte value.
 *
 * @param dst Region to fill
 * @param c Byte value to fill
 * @param n Number of bytes to write
 *
 * @return Original dst value
 */
extern "C" void *memset(void *dst, int c, size_t n) {
    if(n != 0) {
        unsigned char *d = reinterpret_cast<unsigned char *>(dst);

        do {
            *d++ = (unsigned char)c;
        } while(--n != 0);
    }

    return dst;
}

/*
 * sizeof(word) MUST BE A POWER OF TWO
 * SO THAT wmask BELOW IS ALL ONES
 */
typedef	long word;      /* "word" used for optimal copy speed */

#define wsize   sizeof(word)
#define wmask   (wsize - 1)

/*
 * Copy a block of memory, handling overlap.
 *
 * @param dst0 Start of the destination region
 * @param src0 Start of the source region
 * @param length Number of bytes to move
 *
 * @return The original dst0 value
 */
void *memmove(void *dst0, const void *src0, size_t length) {
    char *dst = reinterpret_cast<char *>(dst0);
    const char *src = reinterpret_cast<const char *>(src0);
    size_t t;

    if (length == 0 || dst == src) {
        /* nothing to do */
        goto done;
    }

    /*
     * Macros: loop-t-times; and loop-t-times, t>0
     */
#define	TLOOP(s) if (t) TLOOP1(s)
#define	TLOOP1(s) do { s; } while (--t)

    if ((unsigned long)dst < (unsigned long)src) {
        /*
         * Copy forward.
         */
        t = (long)src; /* only need low bits */
        if ((t | (long)dst) & wmask) {
            /*
             * Try to align operands.  This cannot be done
             * unless the low bits match.
             */
            if ((t ^ (long)dst) & wmask || length < wsize) {
                t = length;
            } else {
                t = wsize - (t & wmask);
            }
            length -= t;
            TLOOP1(*dst++ = *src++);
        }
        /*
         * Copy whole words, then mop up any trailing bytes.
         */
        t = length / wsize;
        TLOOP(*(word *)dst = *(const word *)src; src += wsize; dst += wsize);
        t = length & wmask;
        TLOOP(*dst++ = *src++);
    } else {
        /*
         * Copy backwards.  Otherwise essentially the same.
         * Alignment works as before, except that it takes
         * (t&wmask) bytes to align, not wsize-(t&wmask).
         */
        src += length;
        dst += length;
        t = (long)src;
        if ((t | (long)dst) & wmask) {
            if ((t ^ (long)dst) & wmask || length <= wsize) {
                t = length;
            } else {
                t &= wmask;
            }
            length -= t;
            TLOOP1(*--dst = *--src);
        }
        t = length / wsize;
        TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(const word *)src);
        t = length & wmask;
        TLOOP(*--dst = *--src);
    }

done:
    return dst0;
}



/*
 * Copy src to dst, truncating or null-padding to always copy n bytes.
 *
 * @param dst Destination string
 * @param src Source string
 * @param n Number of bytes available in destination
 *
 * @return Start of destination string
 */
char *strncpy(char *dst, const char *src, size_t n) {
    if (n != 0) {
        char *d = dst;
        const char *s = src;

        do {
            if ((*d++ = *s++) == 0) {
                /* NUL pad the remaining n-1 bytes */
                while (--n != 0) {
                    *d++ = 0;
                }
                break;
            }
        } while (--n != 0);
    }
    return dst;
}
