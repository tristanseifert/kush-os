#include "FIFO.h"
#include "SVGA.h"

#include "Log.h"

#include <unistd.h>
#include <svga_reg.h>
#include <svga3d_reg.h>

#include <libpci/Device.h>
#include <sys/syscalls.h>

using namespace svga;
using Err = ::SVGA::Errors;

/// Region of virtual memory in which FIFOs are mapped
static uintptr_t kPrivateMappingRange[2] = {
    // start
    0x11090000000,
    // end
    0x110B0000000,
};

/**
 * Initializes the FIFO. We'll try to map the memory provided in the PCI BAR and initialize its
 * contents as needed.
 */
FIFO::FIFO(SVGA *controller, const libpci::Device::AddressResource &bar) : s(controller) {
    int err;
    uintptr_t base{0};

    // round up size to page multiple
    const auto pageSz = sysconf(_SC_PAGESIZE);
    const auto size = ((bar.length + pageSz - 1) / pageSz) * pageSz;

    // create the phys region
    err = AllocVirtualPhysRegion(bar.base, size, VM_REGION_RW | VM_REGION_MMIO, &this->vmRegion);
    if(err) {
        Warn("%s failed: %d", "AllocVirtualPhysRegion", err);
        this->status = err; return;
    }

    // map it
    err = MapVirtualRegionRange(this->vmRegion, kPrivateMappingRange, size, 0, &base);
    kPrivateMappingRange[0] += size + pageSz;
    if(err) {
        Warn("%s failed: %d", "MapVirtualRegion", err);
        this->status = err; return;
    }

    this->fifo = reinterpret_cast<uint32_t *>(base);
    this->size = bar.length;

    // figure out how much of the FIFO is actually used
    this->size = this->s->regRead(SVGA_REG_MEM_SIZE);
    REQUIRE(this->size >= 0x10000, "FIFO size too small: $%x", this->size);

    /*
     * Initialize the FIFO; the first few words of the FIFO memory are reserved for memory mapped
     * registers. They indicate how much of the FIFO memory is actually used for commands.
     */
    this->fifo[SVGA_FIFO_MIN] = SVGA_FIFO_NUM_REGS * sizeof(uint32_t);
    this->fifo[SVGA_FIFO_MAX] = this->size;
    this->fifo[SVGA_FIFO_NEXT_CMD] = this->fifo[SVGA_FIFO_MIN];
    this->fifo[SVGA_FIFO_STOP] = this->fifo[SVGA_FIFO_MIN];

    /*
     * XXX: this is copied directly from the ref driver; this provides to the host the 3D version
     * we support before negotiation takes place once device is enabled
     */
    if(this->hasCapability(SVGA_CAP_EXTENDED_FIFO) &&
            this->isRegisterValid(SVGA_FIFO_GUEST_3D_HWVERSION)) {
        this->fifo[SVGA_FIFO_GUEST_3D_HWVERSION] = SVGA3D_HWVERSION_CURRENT;
    }

    Trace("FIFO for $%p: %lu bytes, first command at $%x", controller, this->size,
            this->fifo[SVGA_FIFO_MIN]);

    // reserve command size
    this->bounceBuf.reserve(kMaxCommandSize);
}

/**
 * Unmaps the FIFO.
 */
FIFO::~FIFO() {
    if(this->vmRegion) {
        DeallocVirtualRegion(this->vmRegion);
    }
}



/**
 * Checks whether we support a particular FIFO capability.
 */
bool FIFO::hasCapability(const uintptr_t cap) const {
    return !!(this->fifo[SVGA_FIFO_CAPABILITIES] & cap);
}

/**
 * Checks whether a particular FIFO register has been allocated as a register rather than as part
 * of the command buffer region.
 */
bool FIFO::isRegisterValid(const uintptr_t reg) const {
    return this->fifo[SVGA_FIFO_MIN] > (reg * sizeof(uint32_t));
}



/**
 * Reserves memory for a command of the given size in the command FIFO. In most cases, the command
 * is written directly into the FIFO.
 *
 * This hides the fact that the FIFO may wrap around and the command may not be contiguous in
 * memory, in which case we're simply going to write into a temporary (in memory) buffer and copy
 * it into the FIFO.
 *
 * @note You _must_ pair this with calls to commit/commitAll.
 *
 * @param bytes Number of bytes required for the command
 * @param outRange Buffer into which the command may be written if the call is successful; it will
 *        have space for at least `bytes` bytes.
 *
 * @return 0 on success, or an error code.
 */
int FIFO::reserve(const size_t bytes, std::span<std::byte> &outRange) {
    // get the current FIFO state
    volatile uint32_t *fifo = this->fifo;
    uint32_t max = fifo[SVGA_FIFO_MAX];
    uint32_t min = fifo[SVGA_FIFO_MIN];
    uint32_t nextCmd = fifo[SVGA_FIFO_NEXT_CMD];
    bool reserveable = this->hasCapability(SVGA_FIFO_CAP_RESERVE);

    // validate the command length
    if(bytes > (max - min)) {
        return Err::CommandTooLarge;
    }
    else if(bytes % sizeof(uint32_t)) {
        return Err::CommandNotAligned;
    }
    else if(this->reservedSize) {
        return Err::CommandInFlight;
    }

    while(1) {
        uint32_t stop = this->fifo[SVGA_FIFO_STOP];
        bool reserveInPlace{false}, needBounce{false};

        // no pending FIFO data
        if(nextCmd >= stop) {
            // sufficient contiguous space
            if(nextCmd + bytes < max || (nextCmd + bytes == max && stop > min)) {
                reserveInPlace = true;
            }
            // some space available but FIFO is full; wait for emptying
            else if ((max - nextCmd) + (stop - min) <= bytes) {
                this->handleFifoFull();
            }
            // command requires bounce buffer
            else {
                needBounce = true;
            }
        }
        // there is data in the FIFO between nextCmd and max
        else {
            // sufficient contiguous space
            if(nextCmd + bytes < stop) {
                reserveInPlace = true;
            }
            // FIFO too full to accept command
            else {
                this->handleFifoFull();
            }
        }

        /*
         * Ensure that the virtual machine hypervisor can support us using direct writes to the
         * FIFO for command buffers; if not, default to using the bounce buffer.
         */
        if(reserveInPlace) {
            if(reserveable || bytes <= sizeof(uint32_t)) {
                this->usingBounceBuffer = false;
                if(reserveable) {
                    this->fifo[SVGA_FIFO_RESERVED] = bytes;
                }

                outRange = std::span<std::byte>(reinterpret_cast<std::byte *>(nextCmd + this->fifo),
                        bytes);
                return 0;
            }
        } 
        // use bounce buffer as we don't support reservation
        else {
            needBounce = true;
        }

        // use bounce buffer if needed
        if(needBounce) {
            this->usingBounceBuffer = true;

            this->bounceBuf.resize(bytes);
            outRange = this->bounceBuf;
            return 0;
        }

        // otherwise, the FIFO was full and we would like to try again
    }

    // NOTREACHED
}

/**
 * Handles a full FIFO; this performs a legacy style sync against the graphics device FIFO.
 *
 * TODO: extend to use FIFO IRQs
 */
void FIFO::handleFifoFull() {
    this->s->regWrite(SVGA_REG_SYNC, 1);
    this->s->regRead(SVGA_REG_BUSY);
}
