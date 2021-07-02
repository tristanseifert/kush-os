#ifndef KERNEL_CRYPTO_RANDOMPOOL_H
#define KERNEL_CRYPTO_RANDOMPOOL_H

#include <stdint.h>
#include <runtime/LockFreeQueue.h>

#include <arch/spinlock.h>

#include "sha2.h"

namespace crypto {
class Random;

/**
 * The random pool accumulates entropy from various sources in one of 32 pools; sources shall cycle
 * between all of them when adding events.
 *
 * As events are added to the random pool, they're hashed into that pool's state. When a pool is
 * used as part of computing new seed material, its hash is zeroed out.
 */
class RandomPool {
    /// Minimum entropy required in the lowest pool for it to be used
    constexpr static const size_t kMinEntropy{128};
    /// Maximum number of bytes of entropy that may be added in one call, in bytes
    constexpr static const size_t kMaxEntropyLen{SHA256_BLOCK_SIZE};

    public:
        /**
         * Defines the different sources from which entropy data can be derived; this is encoded
         * into the entropy as it's added to the pool.
         */
        enum class SourceId: uint8_t {
            Interrupt                   = 0x01,
            Scheduler                   = 0x02,

            /// User provided entropy
            External                    = 0xFF,
        };

    public:
        /// Initialize the global entropy pool instance.
        static void Init();
        /// Gets the global entropy pool instance
        static inline RandomPool *the() {
            return gShared;
        }

        /// Adds entropy to the given pool.
        void add(const SourceId from, const uint8_t pool, const void *data, const size_t dataLen);

        /// Calculates a new 32 byte seed for the RNG based on available entropy.
        [[nodiscard]] bool get(void *outPtr, const size_t outLen);

        /// Test whether the pool has enough entropy to support reseeding
        bool isReady() const {
            return this->pools[0].bytesAvailable >= kMinEntropy;
        }

    private:
        /**
         * Container for the structures associated with the pool. This really just consists of the
         * SHA hashing context that we incrementally hash events into.
         */
        struct Pool {
            /// Lock to protect data structures
            DECLARE_SPINLOCK(lock);
            /// Hashing context
            sha256_ctx sha;

            /**
             * Number of bytes of actual provided entropy that have been hashed into the state of
             * this pool. Note that this does NOT include the source/length header that each piece
             * of entropy has prepended before it's hashed into the state.
             */
            size_t bytesAvailable{0};

            /// Ensure the SHA context is reset by default
            Pool() {
                this->reset();
            }
            /// Reinitializes the SHA context
            inline void reset() {
                memset(&this->sha, 0, sizeof(this->sha));
                sha256_init(&this->sha);
                this->bytesAvailable = 0;
            }
        };

    private:
        static RandomPool *gShared;

        /// Lock used to serialize reseeding
        DECLARE_SPINLOCK(getLock);

        /**
         * Reseeding counter; this is incremented whenever we reseed the generator and it is used
         * to determine from which entropy pools we gather data.
         */
        uint32_t reseedCount{0};

        /**
         * Entropy pools; these are used incrementally to generate a new seed, based on the value
         * of the counter.
         */
        Pool pools[32];
};
}

#endif
