#include <string.h>

/*
 * Portions of this code are taken from the OpenBSD libc, released under the
 * following license:
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Finds the first occurrence of value in the first num bytes of ptr.
 */
const void *memchr(const void *ptr, const uint8_t value, const size_t num) {
    const uint8_t *read = (const uint8_t *) ptr;

    for(int i = 0; i < num; i++) {
        if(read[i] == value) return &read[i];
    }

    return NULL;
}

/*
 * Compares the first num bytes in two blocks of memory.
 *
 * Returns 0 if equal, a value greater than 0 if the first byte in ptr1 is greater than the first
 * byte in ptr2; and a value less than zero if the opposite. Note that these comparisons are
 * performed on uint8_t types.
 */
int memcmp(const void *ptr1, const void *ptr2, const size_t num) {
    const uint8_t *read1 = (const uint8_t *) ptr1;
    const uint8_t *read2 = (const uint8_t *) ptr2;

    for(int i = 0; i < num; i++) {
        if(read1[i] != read2[i]) {
            if(read1[i] > read2[i]) return 1;
            else return -1;
        }
    }

    return 0;
}

/*
 * Copies num bytes from source to destination.
 */
void *memcpy(void *destination, const void *source, const size_t num) {
    uint32_t *dst = (uint32_t *) destination;
    const uint32_t *src = (const uint32_t *) source;

    int i = 0;
    for(i = 0; i < num/4; i++) {
        *dst++ = *src++;
    }

    // If we have more bytes to copy, perform the copy
    if((i * 4) != num) {
        uint8_t *dst_byte = (uint8_t *) dst;
        const uint8_t *src_byte = (const uint8_t *) src;

        for(int x = (i * 4); x < num; x++) {
            *dst_byte++ = *src_byte++;
        }
    }

    return destination;
}

/*
 * Fills a given segment of memory with a specified value.
 */
void *memset(void *ptr, const uint8_t value, const size_t num) {
    if(value == 0x00) {
        return memclr(ptr, num);
    }

    uint8_t *write = (uint8_t *) ptr;

    for(int i = 0; i < num; i++) {
        write[i] = value;
    }

    return ptr;
}

/*
 * Clears count bytes of memory, starting at start, with 0x00.
 */
void* memclr(void *start, const size_t count) {
    // Programmer is an idiot
    if(!count) return start;

    __asm__("rep stosl;"::"a"(0),"D"((size_t) start),"c"(count / 4));
    __asm__("rep stosb;"::"a"(0),"D"(((size_t) start) + ((count / 4) * 4)),"c"(count - ((count / 4) * 4)));

    return (void *) ((size_t) start + count);
}

/**
 * Moves the given memory region; they can overlap. A temporary stack buffer is used.
 */
void *memmove(void *dest, const void *src, const size_t n) {
    unsigned char tmp[n];
    memcpy(tmp, src, n);
    memcpy(dest, tmp, n);
    return dest;
}



/*
 * Compares n bytes of the two strings.
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    while(n--) {
        if(*s1++!=*s2++) {
            return *(const unsigned char*)(s1 - 1) - *(const unsigned char*)(s2 - 1);
        }
    }

    return 0;
}

/*
 * Copies n bytes from the source string to the destination buffer, filling the
 * destination with zeros if source ends prematurely.
 */
char *strncpy(char *dest, const char *src, size_t n) {
    char *ret = dest;
    do {
        if (!n--) {
            return ret;
        }
    } while ((*dest++ = *src++));

    while (n--) {
        *dest++ = 0;
    }

    return ret;
}
