#include "Random.h"
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

    REQUIRE(this->isReady, "RNG not ready");

    // calculate number of 16 byte blocks
    if(!outDataBytes) return;
    const auto numBlocks = (outDataBytes + 15) / 16;

    size_t writeBytesRemaining = outDataBytes;
    auto writePtr = reinterpret_cast<uint8_t *>(outData);

    for(size_t i = 0; i < numBlocks; i++) {
        // generate data
        memcpy(scratch, this->counter, AES_BLOCKLEN);
        AES_CTR_xcrypt_buffer(&this->aesCtx, scratch, AES_BLOCKLEN);

        // copy out data
        const auto nb = (writeBytesRemaining > AES_BLOCKLEN) ? AES_BLOCKLEN : writeBytesRemaining;
        memcpy(writePtr, scratch, nb);
        writePtr += nb;

        // update state for next round
        ++(*this);
    }

    // rekey and clean up
    memset(scratch, 0, sizeof(scratch));

    this->rekey();
}

/**
 * Seeds the generator. This hashes the provided seed, uses it as the new round key, and increments
 * the counter.
 *
 * XXX: Is doing a single round of SHA-256 a good way of computing the initial key, or is there a
 * key derivation function that's better suited?
 */
void Random::seed(const void *data, const size_t dataBytes) {
    sha256_ctx ctx;

    memset(&ctx, 0, sizeof(ctx));
    sha256_init(&ctx);

    // generate new key
    uint8_t digest[SHA256_DIGEST_SIZE]{0};

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
 */
void Random::rekey() {
    uint8_t scratch[AES_BLOCKLEN], newKey[32];

    // generate first 16 bytes
    memcpy(scratch, this->counter, AES_BLOCKLEN);
    AES_CTR_xcrypt_buffer(&this->aesCtx, scratch, AES_BLOCKLEN);
    memcpy(newKey, scratch, AES_BLOCKLEN);
    ++(*this);

    // generate second 16 bytes
    memcpy(scratch, this->counter, AES_BLOCKLEN);
    AES_CTR_xcrypt_buffer(&this->aesCtx, scratch, AES_BLOCKLEN);
    memcpy(newKey+AES_BLOCKLEN, scratch, AES_BLOCKLEN);
    ++(*this);

    // copy the key and clean up
    memcpy(this->key, newKey, sizeof(newKey));

    memset(newKey, 0, sizeof(newKey));
    memset(scratch, 0, sizeof(scratch));
}
