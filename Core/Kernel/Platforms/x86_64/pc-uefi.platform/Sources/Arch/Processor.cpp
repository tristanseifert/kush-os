#include "Processor.h"

using namespace Platform::Amd64Uefi;

/**
 * Halts all processors in the system.
 *
 * This is accomplished by sending an IPI to each processor, waiting until they all reply (or a
 * timeout expires) and then halting ourselves.
 */
[[noreturn]] void Processor::HaltAll() {
    // TODO: IPI stuff

    // we're done, halt ourselves
    HaltSelf();
}
