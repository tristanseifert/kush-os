#ifndef KERNEL_CRYPTO_RANDOM_H
#define KERNEL_CRYPTO_RANDOM_H

#include "aes.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <arch/spinlock.h>

namespace crypto {
/**
 * Provides random number generation for the kernel.
 *
 * This is a basic implementation of the Fortuna CSPRNG, using AES with a 256 bit key size (to
 * avoid some statistical issues with 128 bit keys) as the block cipher.
 *
 * TODO: Evaluate how good this actually is
 */
class Random {
    /**
     * Maximum size of a single randomness request, in bytes. This is sized such that we can hide
     * output block collisions before rekeying, meaning we want about 2^16 blocks (assuming that
     * the cipher will likely produce a duplicate block every 2^64) as the max.
     */
    constexpr static const size_t kMaxRequest{65536 * AES_BLOCKLEN};

    /**
     * Minimum delay between times that the generator is reseeded, in nanoseconds.
     */
    constexpr static const uint64_t kMinReseedInterval{100000000ULL};

    public:
        /// Initialize the global random number generator instance.
        static void Init();
        /// Gets the global random generator instance
        static inline Random *the() {
            return gShared;
        }

        ~Random();

        /// Secrete the given number of bytes of random data.
        void operator()(void *outData, const size_t outDataBytes);

    private:
        Random();

        /// Increments the internal block counter
        void operator++();

        /// Reseeds the generator if needed
        void reseedIfNeeded();
        /// Reseed the generator
        void seed(const void *data, const size_t dataBytes);
        /// Regenerates the key used for producing output data.
        void rekey();

        /**
         * Generate a block of data; the counter is incremented as well.
         *
         * @param scratch A buffer at least AES_BLOCKLEN bytes in size; you should zero this when
         *        the calling function is done.
         * @param ptr Buffer to copy the generated block into.
         * @param bytes Number of bytes to copy out; must be at most AES_BLOCKLEN
         */
        inline void generateBlock(uint8_t *scratch, void *ptr, const size_t bytes) {
            memcpy(scratch, this->counter, AES_BLOCKLEN);
            AES_CTR_xcrypt_buffer(&this->aesCtx, scratch, AES_BLOCKLEN);
            memcpy(ptr, scratch, bytes);

            // increment counter
            ++(*this);
        }

    private:
        static Random *gShared;

        DECLARE_SPINLOCK(lock);

        /**
         * AES context used for encrypting blocks produced by the generator; this operates with
         * 256 bit keys.
         */
        struct AES_ctx aesCtx;

        /**
         * Block counter; this is a 128 integer value that's incremented for every 16 byte block
         * that we produce from the PRNG.
         */
        uint8_t counter[16];

        /**
         * Key used for AES encryption of blocks from the PRNG. This is a 256 bit key generated
         * from the initial seed and subsequent random read operations.
         */
        uint8_t key[32];

        /// Timestamp at which the generator was last reseeded
        uint64_t lastReseed{0};

        /// Whether the RNG has been initialized
        bool isReady{false};
};
}

#endif
