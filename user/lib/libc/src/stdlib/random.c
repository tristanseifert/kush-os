#include <stdlib.h>

/**
 * Returns a random 32-bit value shoved into the long.
 */
long random() {
    return arc4random();
}

/**
 * Returns a random integer.
 */
int rand() {
    return arc4random_uniform(RAND_MAX);
}
