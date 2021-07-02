#include "RandomPool.h"
#include "Random.h"
#include "sha2.h"

#include <log.h>
#include <string.h>

#include <new>

using namespace crypto;

/// Buffer for the entropy pool
static uint8_t __attribute__((aligned(alignof(RandomPool)))) gSharedBuf[sizeof(RandomPool)];
/// Pointer to the global entropy pool instance
RandomPool *RandomPool::gShared{nullptr};

/**
 * Initializes the global entropy pool in the preallocated buffer.
 */
void RandomPool::Init() {
    auto ptr = reinterpret_cast<RandomPool *>(&gSharedBuf);
    gShared = new(ptr) RandomPool;
}

/**
 * Inserts data into the random pool.
 */
void RandomPool::add(const SourceId from, const uint8_t poolId, const void *data,
        const size_t dataLen) {
    // validate arguments and get the pool
    REQUIRE(data, "invalid entropy ptr");
    REQUIRE(dataLen && dataLen < kMaxEntropyLen, "invalid entropy length %lu", dataLen);

    REQUIRE(poolId < 32, "invalid random pool %u", poolId);
    auto &pool = this->pools[poolId];

    // build the event buffer
    uint8_t buffer[kMaxEntropyLen + 2];
    buffer[0] = static_cast<uint8_t>(from);
    buffer[1] = dataLen;
    memcpy(buffer+2, data, dataLen);

    // update the hash with the data
    SPIN_LOCK_GUARD(pool.lock);

    sha256_update(&pool.sha, buffer, 2 + dataLen);
    pool.bytesAvailable += dataLen;
}

/**
 * Generates a new seed from available entropy.
 *
 * Pools are used based on whether the reseed counter is a divisor of 2^i, where i is its index.
 *
 * The seed is produced by concatenating the hashes of each of the pools that are used, then
 * calculating a hash over this string and writing that out as the seed.
 *
 * @return Whether we suceeded in recalculating the seed
 */
bool RandomPool::get(void *outPtr, const size_t outLen) {
    uint8_t scratch[SHA256_DIGEST_SIZE];

    // validate args
    REQUIRE(outPtr, "invalid entropy ptr");
    REQUIRE(outLen >= SHA256_DIGEST_SIZE, "invalid entropy length %lu", outLen);

    // set up the SHA context
    sha256_ctx sha;
    memset(&sha, 0, sizeof(sha));
    sha256_init(&sha);

    // lock state and ensure we're valid
    SPIN_LOCK_GUARD(this->getLock);
    const auto count = ++this->reseedCount;

    if(!this->isReady()) return false;

    // process all pools
    for(size_t i = 0; i < 32; i++) {
        // skip pool if not a divisor
        const auto bit{1 << i};
        if(i && (count % bit)) continue;

        // complete its hash and yeet it into our state
        auto &p = this->pools[i];

        SPIN_LOCK(p.lock);
        sha256_final(&p.sha, scratch);
        p.reset();
        SPIN_UNLOCK(p.lock);

        sha256_update(&sha, scratch, sizeof(scratch));
    }

    // copy out the final hash and clean up
    sha256_final(&sha, reinterpret_cast<uint8_t *>(outPtr));

    return true;
}
