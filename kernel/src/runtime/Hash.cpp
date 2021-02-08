/**
 * Components below are MurmurHash3 code:
 *
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 */
#include "Hash.h"

#include <stdint.h>

using namespace rt;


//-----------------------------------------------------------------------------
// Platform-specific functions and macros
#define FORCE_INLINE __attribute__((always_inline)) inline

static FORCE_INLINE uint32_t rotl32 ( uint32_t x, int8_t r ) {
    return (x << r) | (x >> (32 - r));
}

/*static FORCE_INLINE uint64_t rotl64 ( uint64_t x, int8_t r ) {
    return (x << r) | (x >> (64 - r));
}*/

#define	ROTL32(x,y)                     rotl32(x,y)
#define ROTL64(x,y)                     rotl64(x,y)

#define BIG_CONSTANT(x)                 (x##LLU)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

#define getblock(p, i) (p[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static FORCE_INLINE uint32_t fmix32 ( uint32_t h ) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

/*static FORCE_INLINE uint64_t fmix64 ( uint64_t k ) {
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return k;
}*/

//-----------------------------------------------------------------------------
/**
 * Secretes a 32-bit long hash for the given input.
 */
static void MurmurHash3_x86_32(const void * key, const size_t len, const uint32_t seed, void *out) {
    const uint8_t * data = (const uint8_t*)key;
    const int nblocks = len / 4;
    int i;

    uint32_t h1 = seed;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    //----------
    // body
    const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

    for(i = -nblocks; i; i++) {
        uint32_t k1 = getblock(blocks,i);

        k1 *= c1;
        k1 = ROTL32(k1,15);
        k1 *= c2;

        h1 ^= k1;
        h1 = ROTL32(h1,13); 
        h1 = h1*5+0xe6546b64;
    }

    //----------
    // tail
    const uint8_t * tail = (const uint8_t*)(data + nblocks*4);
    uint32_t k1 = 0;

    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16; [[fallthrough]];
        case 2: k1 ^= tail[1] << 8; [[fallthrough]];
        case 1: k1 ^= tail[0];
              k1 *= c1; k1 = ROTL32(k1,15); k1 *= c2; h1 ^= k1;
    };

    //----------
    // finalization
    h1 ^= len;
    h1 = fmix32(h1);

    *(uint32_t*)out = h1;
}

/**
 * Generic hash function
 */
const uint32_t rt::hash(const void *in, const size_t inLen, const uint32_t seed) {
    uint32_t temp;
    MurmurHash3_x86_32(in, inLen, seed, &temp);
    return temp;
}

