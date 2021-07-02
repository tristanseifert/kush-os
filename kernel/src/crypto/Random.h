#ifndef KERNEL_CRYPTO_RANDOM_H
#define KERNEL_CRYPTO_RANDOM_H

#include "aes.h"

#include <stddef.h>
#include <stdint.h>

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
    public:
        /// Initialize the global random number generator instance.
        static void Init();
        /// Gets the global random generator instance
        static inline Random *the() {
            return gShared;
        }

        Random();
        ~Random();

        /// Secrete the given number of bytes of random data.
        void operator()(void *outData, const size_t outDataBytes);

        /// Reseed the generator
        void seed(const void *data, const size_t dataBytes);
        /// Regenerates the key used for producing output data.
        void rekey();

    private:
        /// Increments the internal block counter
        void operator++();

    private:
        static Random *gShared;

        /// Whether the RNG has been initialized
        bool isReady{false};

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
};
}

#endif
