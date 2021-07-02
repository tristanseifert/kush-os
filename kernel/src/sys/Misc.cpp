#include "Handlers.h"

#include "crypto/Random.h"

#include <log.h>

using namespace sys;

/// Maximum size of entropy that can be acquired from the kernel (bytes)
constexpr static const size_t kMaxEntropy{256};

/**
 * Reads the given number of bytes (up to the specified maximum) from the kernel's random number
 * generator.
 */
intptr_t sys::GetEntropy(void *outPtr, const size_t outBytes) {
    // validate arguments
    if(outBytes > kMaxEntropy) {
        return Errors::InvalidArgument;
    }
    else if(!Syscall::validateUserPtr(outPtr, outBytes)) {
        return Errors::InvalidPointer;
    }

    const auto toGenerate = outBytes;

    // acquire
    uint8_t buf[kMaxEntropy];
    (*crypto::Random::the())(buf, toGenerate);

    // copy out
    Syscall::copyOut(buf, toGenerate, outPtr, outBytes);
    return outBytes;
}

