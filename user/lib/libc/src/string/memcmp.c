#include <string.h>
#include <stdint.h>

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
