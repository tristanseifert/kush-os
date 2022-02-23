#include "Exceptions/Handler.h"

#include "Logging/Console.h"

#include <Platform.h>

using namespace Kernel::Exceptions;

/**
 * Dispatches an exception.
 *
 * Whichever handler is invoked is responsible for properly dealing with the exception, which may
 * include altering the return addresses in the processor state, terminating the offending task,
 * or panicking the system.
 *
 * @param type Exception type; this defines the format (if any) of the `auxData` field.
 * @param state Processor register state at the time of the exception
 * @param auxData An optional pointer to auxiliary data.
 */
void Handler::Dispatch(const ExceptionType type, Platform::ProcessorState &state, void *auxData) {
    // TODO: implement

    // for now, just print what happened
    constexpr static const size_t kStateBufSz{512};
    static char stateBuf[kStateBufSz];
    Platform::ProcessorState::Format(state, stateBuf, kStateBufSz);

    constexpr static const size_t kBacktraceBufSz{1024};
    static char backtraceBuf[kBacktraceBufSz];
    int frames = Platform::ProcessorState::Backtrace(state, backtraceBuf, kBacktraceBufSz);

    PANIC("Unhandled exception $%08x, aux = %p\n%s\nState backtrace: %s", (uint32_t) type,
            auxData, stateBuf, (frames > 0) ? backtraceBuf : nullptr);
}

