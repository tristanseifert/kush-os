#ifndef ARCH_X86_64_THREADSTATE_H
#define ARCH_X86_64_THREADSTATE_H

// we have to manually define structs offsets for our assembler code
#include "arch/ThreadState.h"
#define TS_OFF_STACKTOP                 0
#define TS_OFF_FPU_SHOULD_RESTORE       8
#define TS_OFF_FPU_STATE_PTR            16
#define TS_OFF_REGS                     32

#ifndef ASM_FILE
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace arch {
/**
 * Registers for a kernel thread as saved on the stack.
 */
struct CpuRegs {
    // registers added for 64-bit mode
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    // basic registers (in common with 32-bit mode, in same order as PUSHA)
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    // CPU flags
    uint64_t rflags;
    // instruction pointer/return address
    uint64_t rip;
} __attribute__((packed));

/**
 * Processor state for an x86_64 thread.
 * 
 * This includes an area for the floating point state. We only save this state when the thread is
 * context switched out if the "FPU used" flag is set. We'll periodically clear this flag if
 * the "FPU count resets" counter is below a certain theshold. So, if a thread uses the floating
 * point hardware every time it's context switched in, after that counter exceeds this value
 * we'll just proceed to always save/restore the state because we assume the thread will likely
 * use the FPU again.
 *
 * @note The offsets of these values are critical! They're used from the assembly routines.
*/
struct ThreadState {
    /// Stack pointer to return to
    void *stackTop = nullptr;

    /// when set, the FPU has been used and its state should be restored
    bool fpuShouldRestore{false};
    /// XSAVE data area for floating point state
    void *fpuState{nullptr};
    /// number of times we've taken an FPU fault in this thread
    size_t fpuFaults{0};

    /// saved thread state
    CpuRegs saved;

    /// FS base for the thread
    uintptr_t fsBase{0};
    /// GS Base for the thread
    uintptr_t gsBase{0};
};

/**
 * Task state specific to an x86_64 processor
 */
struct TaskState {
    /**
     * IO permission bitmap, or `nullptr` if the task is not allowed to make any IO calls. Unlike
     * on 32-bit Intel, we can't use the TSS' IOPB so all IO accesses are performed via syscalls
     * (because they're reasonably infrequent) and this bitmap serves as an allow list for IO port
     * accesses.
     *
     * When a task requests the IO privilege, we'll allocate an 8K bitmap (for the 64K IO address
     * space) which is initially set to all 0's, indicating no IO ports may be accessed. The task
     * may then add or remove ports to this list via syscalls, until the bitmap is locked, at
     * which point no further port rights can be added: they can only be removed.
     */
    uint8_t *ioBitmap = nullptr;
    /// whether the IO bitmap has been locked
    bool ioBitmapLocked = false;

    /**
     * Releases any buffers and other resources allocated for the task.
     */
    ~TaskState() {
        if(this->ioBitmap) {
            delete[] this->ioBitmap;
        }
    }

    /**
     * Initializes the IO permissions bitmap.
     *
     * @return true if successful, false if memory allocation failed
     */
    bool initIoPermissions() {
        // disallow reallocation
        if(this->ioBitmap) return false;

        // allocate the buffer
        auto map = new uint8_t[kIopbBytes];
        if(!map) return false;

        // zero it and save
        memset(map, 0, kIopbBytes);
        this->ioBitmap = map;

        return true;
    }

    /**
     * Locks the IO permissions bitmap.
     */
    void lockIoPermissions() {
        __atomic_store_n(&this->ioBitmapLocked, true, __ATOMIC_RELAXED);
    }

    /**
     * Test whether a particular IO port range can be accessed.
     *
     * @param base IO port base to test for access permissions
     * @param len Length of IO port range to test
     *
     * @return Whether the access is allowed
     */
    bool testIoRange(const uint16_t base, const uint16_t len) const {
        // ensure we've got an IO bitmap, and that the inputs are valid
        if(!this->ioBitmap) return false;
        else if(!len) return false;
        else if((static_cast<size_t>(base) + static_cast<size_t>(len)) > UINT16_MAX) return false;

        // test the appropriate number of bits (common optimizations)
        switch(len) {
            // byte
            case 1:
                return this->testIoPort(base    );
            // word
            case 2:
                return this->testIoPort(base    ) && this->testIoPort(base + 1);
            // dword
            case 4:
                return this->testIoPort(base    ) && this->testIoPort(base + 1) &&
                       this->testIoPort(base + 2) && this->testIoPort(base + 3);

            // unknown length, do a slow test
            default:
                for(size_t i = base; i < (base + len); i++) {
                    const auto byte = this->ioBitmap[i / 8];
                    if(!(byte & (1 << (1 % 8)))) return false;
                }

                return true;
        }
    }

    /**
     * Add an IO port range to the allow list so it can be accessed.
     *
     * @note Once the bitmap is locked, it's not possible to add any new IO ports to the list.
     *
     * @param base First IO port address
     * @param len Number of subsequent IO port addresses to add to list
     *
     * @return Whether the IO port range was successfully added to the allow list
     */
    bool allowIoRange(const uint16_t base, const uint16_t len) {
        // validate inputs
        if(!this->ioBitmap || this->ioBitmapLocked) return false;
        else if(!len) return false;
        else if((static_cast<size_t>(base) + static_cast<size_t>(len)) > UINT16_MAX) return false;

        // mark as allowed (set the bit)
        for(size_t i = base; i < (base + len); i++) {
            this->ioBitmap[i / 8] |= (1 << (i % 8));
        }

        return true;
    }

    /**
     * Removes an IO port range from the IO allow list.
     *
     * @note This call is allowed after the access bitmap has been locked.
     *
     * @param base First IO port address
     * @param len Number of subsequent IO port addresses to remove from list
     *
     * @return Whether the IO port range was removed from the allow list
     */
    bool disallowIoRange(const uint16_t base, const uint16_t len) {
        // validate inputs
        if(!this->ioBitmap) return false;
        else if(!len) return false;
        else if((static_cast<size_t>(base) + static_cast<size_t>(len)) > UINT16_MAX) return false;

        // mark as prohibited (mask off bit)
        for(size_t i = base; i < (base + len); i++) {
            this->ioBitmap[i / 8] &= ~(1 << (i % 8));
        }

        return true;
    }

    private:
        constexpr static const size_t kIopbBytes = (65536 / 8);

    private:
        /**
         * Tests whether a single IO port is accessible. No sanity checking is performed; it's
         * assumed the IO bitmap exists.
         */
        constexpr inline bool testIoPort(const uint16_t base) const {
            return (this->ioBitmap[base / 8] & (1 << (base % 8))) != 0;
        }
};

}

// ensure the manually defined offsets are right
static_assert(offsetof(arch::ThreadState, stackTop) == TS_OFF_STACKTOP, "TS_OFF_STACKTOP wrong");
static_assert(offsetof(arch::ThreadState, fpuShouldRestore) == TS_OFF_FPU_SHOULD_RESTORE,
        "TS_OFF_FPU_SHOULD_RESTORE wrong");
static_assert(offsetof(arch::ThreadState, fpuState) == TS_OFF_FPU_STATE_PTR, "TS_OFF_FPU_STATE_PTR wrong");
static_assert(offsetof(arch::ThreadState, saved) == TS_OFF_REGS, "TS_OFF_REGS wrong");

#endif // ASM_FILE
#endif
