/*
 * Various helpers to work with strings.
 *
 * These functions were, unless specified otherwise, borrowed from the OpenBSD libc.
 */
#ifndef KERNEL_PLATFORM_UEFI_UTIL_STRING_H
#define KERNEL_PLATFORM_UEFI_UTIL_STRING_H

#include <stddef.h>
#include <limits.h>

/// Internal helper functions
namespace Platform::Amd64Uefi::Util {
/**
 * Is the character a space?
 */
constexpr inline int isspace(int c) {
    return c == ' ' || (unsigned) c - '\t' < 5;
}

/**
 * Is the character alphanumeric?
 */
int isalpha(int c) {
    return ((unsigned) c | 32) - 'a' < 26;
}

/**
 * Is the character a digit?
 */
int isdigit(int c) {
    return (unsigned) c - '0' < 10;
}

/**
 * Is the character uppercase?
 */
int isupper(int c) {
    return (unsigned)c - 'A' < 26;
}

/**
 * Is the character a hexadecimal digit?
 */
constexpr inline int isxdigit(int c) {
    return isdigit(c) || ((unsigned)c | 32) - 'a' < 6;
}

/**
 * Compare the two strings, with at most `n` characters.
 */
int strncmp(const char *s1, const char *s2, size_t n) {
    if(!n)
        return 0;

    do {
        if (*s1 != *s2++)
            return (*(const unsigned char *)s1 - *(const unsigned char *)--s2);
        if (*s1++ == 0)
            break;
    } while (--n != 0);

    return 0;
}

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
long strtol(const char *nptr, const char **endptr, int base) {
    const char *s;
    long acc, cutoff;
    int c;
    int neg, any, cutlim;

    /*
     * Ensure that base is between 2 and 36 inclusive, or the special
     * value of 0.
     */
    if (base < 0 || base == 1 || base > 36) {
        if (endptr != 0)
            *endptr = (const char *)nptr;
        // errno = EINVAL;
        return 0;
    }

    /*
     * Skip white space and pick up leading +/- sign if any.
     * If base is 0, allow 0x for hex and 0 for octal, else
     * assume decimal; if base is already 16, allow 0x.
     */
    s = nptr;
    do {
        c = (unsigned char) *s++;
    } while (isspace(c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) && c == '0' &&
        (*s == 'x' || *s == 'X') && isxdigit((unsigned char)s[1])) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;

    /*
     * Compute the cutoff value between legal numbers and illegal
     * numbers.  That is the largest legal value, divided by the
     * base.  An input number that is greater than this value, if
     * followed by a legal input character, is too big.  One that
     * is equal to this value may be valid or not; the limit
     * between valid and invalid numbers is then based on the last
     * digit.  For instance, if the range for longs is
     * [-2147483648..2147483647] and the input base is 10,
     * cutoff will be set to 214748364 and cutlim to either
     * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
     * a value > 214748364, or equal but the next digit is > 7 (or 8),
     * the number is too big, and we will return a range error.
     *
     * Set any if any `digits' consumed; make it negative to indicate
     * overflow.
     */
    cutoff = neg ? LONG_MIN : LONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    if (neg) {
        if (cutlim > 0) {
            cutlim -= base;
            cutoff += 1;
        }
        cutlim = -cutlim;
    }
    for (acc = 0, any = 0;; c = (unsigned char) *s++) {
        if (isdigit(c))
            c -= '0';
        else if (isalpha(c))
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else
            break;
        if (c >= base)
                break;
        if (any < 0)
                continue;
        if (neg) {
            if (acc < cutoff || (acc == cutoff && c > cutlim)) {
                any = -1;
                acc = LONG_MIN;
                // errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc -= c;
            }
        } else {
            if (acc > cutoff || (acc == cutoff && c > cutlim)) {
                any = -1;
                acc = LONG_MAX;
                // errno = ERANGE;
            } else {
                any = 1;
                acc *= base;
                acc += c;
            }
        }
    }
    if (endptr != 0)
        *endptr = (const char *) (any ? s - 1 : nptr);
    return (acc);
}
}

#endif
