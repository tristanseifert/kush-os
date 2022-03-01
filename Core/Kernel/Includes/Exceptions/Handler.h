#ifndef KERNEL_EXCEPTIONS_HANDLER_H
#define KERNEL_EXCEPTIONS_HANDLER_H

#include <stdint.h>

#include <platform/Processor.h>

/// Platform-agnostic exception handling
namespace Kernel::Exceptions {
/**
 * @brief Entry point from platform-specific exception handler
 *
 * The exception handler is responsible for dispatching generic exceptions into the rest of the
 * kernel and its subsystems. These generic exceptions correspond roughly to the following
 * categories:
 *
 * - Arithmetic: Divide-by-zero, overflow, floating point exception, SIMD exception
 * - Instruction: Invalid opcode, protection fault
 * - Memory: Page fault, alignment fault
 * - Debugging: Breakpoints, watchpoints, etc
 */
class Handler {
    public:
        /**
         * @brief Predefined exception types
         *
         * Other platform-specific exceptions can be specified by adding the unique platform
         * exception index to the `PlatformSpecific` value.
         */
        enum class ExceptionType: uint32_t {
            /// Division by zero
            DivideByZero                        = 0x00001000,
            /// Arithmetic overflow (or explicit overflow checks)
            Overflow                            = 0x00001001,
            /// Floating point exception
            FloatingPoint                       = 0x00001002,
            /// SIMD floating point error
            SIMD                                = 0x00001003,
            /// Invalid opcode
            InvalidOpcode                       = 0x00002000,
            /// Protection fault (access violation)
            ProtectionFault                     = 0x00002001,
            /// Page fault on memory access
            PageFault                           = 0x00003000,
            /// Unaligned access
            AlignmentFault                      = 0x00003001,
            /// Debug breakpoint hit
            DebugBreakpoint                     = 0x00004000,

            /// Exception types above this one are platform specific
            PlatformSpecific                    = 0x80000000,
        };

        static void Dispatch(const ExceptionType type, Platform::ProcessorState &state,
                void *auxData = nullptr);
        [[noreturn]] static void AbortWithException(const ExceptionType type,
                Platform::ProcessorState &state, void *auxData = nullptr);
};
}

#endif
