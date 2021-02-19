#include <string.h>
#include <stdint.h>

/*
 * Clears count bytes of memory, starting at start, with 0x00.
 */
void* memclr(void *start, const size_t count) {
    // Programmer is an idiot
    if(!count) return start;

    // TODO: add handlers for non-x86!
    __asm__("rep stosl;"::"a"(0),"D"((size_t) start),"c"(count / 4));
    __asm__("rep stosb;"::"a"(0),"D"(((size_t) start) + ((count / 4) * 4)),"c"(count - ((count / 4) * 4)));

    return (void *) ((size_t) start + count);
}
