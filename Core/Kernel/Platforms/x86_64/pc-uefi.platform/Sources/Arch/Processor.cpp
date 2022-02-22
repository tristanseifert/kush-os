#include "Processor.h"

#include <Runtime/Printf.h>

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

/**
 * Formats the processors' register state to the string buffer provided.
 *
 * @param state Registers to read
 * @param out Output string buffer
 * @param outSize Number of characters of space in the buffer
 *
 * @return Number of characters written
 */
int Processor::Regs::Format(const Regs &state, char *out, const size_t outSize) {
    return snprintf_(out, outSize,
            "RAX %016llx RBX %016llx\n"
            "RCX %016llx RDX %016llx\n"
            "RSI %016llx RDI %016llx\n"
            "RBP %016llx  R8 %016llx\n"
            " R9 %016llx R10 %016llx\n"
            "R11 %016llx R12 %016llx\n"
            "R13 %016llx R14 %016llx\n"
            "R15 %016llx\n"
            "RIP %04x:%016llx RSP %04x:%16llx\n"
            "RFLAGS %016llx Error %llx",
            state.rax, state.rbx, state.rcx, state.rdx, state.rsi, state.rdi, state.rbp, state.r8,
            state.r9, state.r10, state.r11, state.r12, state.r13, state.r14, state.r15, state.cs,
            state.rip, state.ss, state.rsp, state.rflags, state.errorCode);
}
