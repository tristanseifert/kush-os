#ifndef KERNEL_RUNTIME_HASH_H
#define KERNEL_RUNTIME_HASH_H

#include <stddef.h>
#include <stdint.h>

namespace rt {
/// General hash function
const uint32_t hash(const void *in, const size_t inLen, const uint32_t seed);

/// Hashes pointers
template<class T>
inline const uint32_t hash(T value, const uint32_t seed = 0);

/// Hashes pointers
template<>
inline const uint32_t hash(void *ptr, const uint32_t seed) {
    return hash(ptr, sizeof(ptr), seed);
}
/// Hashes 32-bit ints
template<>
inline const uint32_t hash(const uint32_t value, const uint32_t seed) {
    return hash(&value, sizeof(value), seed);
}
}

#endif
