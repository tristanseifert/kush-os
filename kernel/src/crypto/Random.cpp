#include "Random.h"
#include "RandomPool.h"
#include "sha2.h"

#include <log.h>
#include <platform.h>
#include <stdint.h>
#include <string.h>

#include <new>

using namespace crypto;

// Buffer for the global shared instance
static uint8_t __attribute__((aligned(alignof(Random)))) gSharedBuf[sizeof(Random)];
// Pointer to the initialized random instance
Random *Random::gShared{nullptr};

/**
 * Initializes the random number generator.
 *
 * @note This is called very early in boot before most of the kernel is initialized! No memory
 * allocations should take place.
 */
void Random::Init() {
    auto ptr = reinterpret_cast<Random *>(&gSharedBuf);
    gShared = new(ptr) Random;

    RandomPool::Init();
}

/**
 * Sets up the random number generator. We'll get the initial entropy from the platform code, hash
 * it and use the resulting 32 bytes to seed the generator.
 *
 * The AES context is set up with an initial all zeros IV.
 */
Random::Random() {
    memset(this->key, 0, sizeof(this->key));
    memset(this->counter, 0, sizeof(this->counter));

    // get initial random data from platform and seed generator with it
    uint8_t temp[32];

    int err = platform::GetEntropy(temp, sizeof(temp));
    REQUIRE(!err, "Failed to get boot entropy: %d", err);

    this->seed(temp, sizeof(temp));

    // set up the AES context
    memset(&this->aesCtx, 0, sizeof(this->aesCtx));
    AES_init_ctx(&this->aesCtx, this->key);

    // clean up
    memset(temp, 0, sizeof(temp));
}

/**
 * Ensures memory containing key material are zeroed.
 */
Random::~Random() {
    memset(&this->aesCtx, 0, sizeof(this->aesCtx));
    memset(this->key, 0, sizeof(this->key));
    memset(this->counter, 0, sizeof(this->counter));
}

/**
 * Increments the internal counter.
 */
void Random::operator++() {
    size_t i{0};
    while(true) {
        if(!(++this->counter[i]) && (++i < 16)) {
            continue;
        }

        return;
    }

    panic("random counter overflowed");
}

/**
 * Generates random bytes and write them to the provided buffer.
 *
 * We'll generate random data in increments of 16 bytes (from the AES block size) and rekey the
 * generator afterwards.
 */
void Random::operator()(void *outData, const size_t outDataBytes) {
    uint8_t scratch[AES_BLOCKLEN];

    SPIN_LOCK_GUARD(this->lock);

    REQUIRE(this->isReady, "RNG not ready");
    REQUIRE(outDataBytes, "RNG request invalid");
    REQUIRE(outDataBytes < kMaxRequest, "RNG request too large: %lu (max %lu)", outDataBytes,
            kMaxRequest);

    this->reseedIfNeeded();

    // calculate number of 16 byte blocks
    const auto numBlocks = (outDataBytes + 15) / 16;

    size_t writeBytesRemaining = outDataBytes;
    auto writePtr = reinterpret_cast<uint8_t *>(outData);

    for(size_t i = 0; i < numBlocks; i++) {
        const auto nb = (writeBytesRemaining > AES_BLOCKLEN) ? AES_BLOCKLEN : writeBytesRemaining;
        this->generateBlock(scratch, writePtr, nb);
        writePtr += nb;
    }

    // rekey and clean up
    memset(scratch, 0, sizeof(scratch));

    this->rekey();
}

/*
 * Check if the generator needs to be reseeded. This will take place when it's been at least as
 * long as the minimum reseed interval, AND the entropy pool has sufficient entropy in P_0.
 *
 * @note The generator must be locked when invoked.
 */
void Random::reseedIfNeeded() {
    auto rp = RandomPool::the();

    // check time and state
    const auto now = platform_timer_now();
    const auto delta = (now - lastReseed);

    if(delta < kMinReseedInterval) return;
    if(!rp->isReady()) return;

    // perform the reseed
    uint8_t newSeed[32];

    if(!rp->get(newSeed, sizeof(newSeed))) return;
    this->seed(newSeed, sizeof(newSeed));

    memset(newSeed, 0, sizeof(newSeed));

    // ensure we don't do this again until needed
    this->lastReseed = now;
}

/**
 * Seeds the generator. This hashes the provided seed, uses it as the new round key, and increments
 * the counter.
 *
 * XXX: Is doing a single round of SHA-256 a good way of computing the initial key, or is there a
 * key derivation function that's better suited?
 *
 * @note The generator must be locked when invoked.
 */
void Random::seed(const void *data, const size_t dataBytes) {
    sha256_ctx ctx;

    memset(&ctx, 0, sizeof(ctx));
    sha256_init(&ctx);

    // generate new key
    uint8_t digest[SHA256_DIGEST_SIZE]{0};

    sha256_update(&ctx, this->key, sizeof(this->key));
    sha256_update(&ctx, reinterpret_cast<const uint8_t *>(data), dataBytes);
    sha256_final(&ctx, digest);

    memcpy(this->key, digest, sizeof(digest));
    memset(digest, 0, sizeof(digest));
    memset(&ctx, 0, sizeof(ctx));

    // update the block counter
    ++(*this);

    this->isReady = true;
}

/**
 * Rekey the generator.
 *
 * @note The generator must be locked when invoked.
 */
void Random::rekey() {
    uint8_t scratch[AES_BLOCKLEN], newKey[32];

    // generate first 16 bytes
    this->generateBlock(scratch, newKey, AES_BLOCKLEN);
    // generate second 16 bytes
    this->generateBlock(scratch, newKey+AES_BLOCKLEN, AES_BLOCKLEN);

    // copy the key and clean up
    memcpy(this->key, newKey, sizeof(newKey));

    memset(newKey, 0, sizeof(newKey));
    memset(scratch, 0, sizeof(scratch));
}
