#include <stdlib.h>

/**
 * Returns a random 32-bit value shoved into the long.
 */
long random() {
    return arc4random();
}
