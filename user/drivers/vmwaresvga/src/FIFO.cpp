#include "FIFO.h"
#include "SVGA.h"

#include "Log.h"

#include <algorithm>

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
    if(kLogCapabilities) Trace("Testing for capability $%08x (have $%08x)",
            this->fifo[SVGA_FIFO_CAPABILITIES], cap);

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

    if(kLogReservations) Trace("Reserving %lu bytes of FIFO (min $%08x, max $%08x, nextCmd $%08x ,"
            "reservable? %c)", bytes, min, max, nextCmd, reserveable ? 'Y' : 'N');

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

    this->reservedSize = bytes;

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
            // driver supports reservation OR it's dword sized
            if(reserveable || bytes <= sizeof(uint32_t)) {
                this->usingBounceBuffer = false;
                if(reserveable) {
                    this->fifo[SVGA_FIFO_RESERVED] = bytes;
                }

                outRange = std::span<std::byte>(reinterpret_cast<std::byte *>(nextCmd + this->fifo),
                        bytes);
                return 0;
            }
            // use bounce buffer as we don't support reservation
            else {
                needBounce = true;
            }
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
 * Reserves memory for a command in the FIFO, which is prefixed by a 32-bit value that
 * indicates the command type.
 *
 * @param type Value to tag the command with
 * @param nBytes Number of bytes of command memory to allocate
 * @param outRange On success, contains a span the command may be written into
 *
 * @return 0 on success or an error code
 */
int FIFO::reserveCommand(const uint32_t type, const size_t nBytes, std::span<std::byte> &outRange){
    // allocate the buffer
    std::span<std::byte> range;
    int err = this->reserve(nBytes + sizeof(type), range);
    if(err) return err;

    // write the header
    memcpy(range.data(), &type, sizeof(type));
    outRange = range.subspan(sizeof(type));
    return 0;
}

/**
 * Reserves memory for an ESCAPE command in the FIFO; these are variable length packets that are
 * used for more advanced SVGA device capabilities.
 *
 * The given number of bytes, plus a 3 dword header, are allocated.
 *
 * @param nsid Namespace identifier for the escape command
 * @param nBytes Number of bytes of command memory to allocate
 * @param outRange On success, contains a span the command may be written into
 *
 * @return 0 on success or an error code
 */
int FIFO::reserveEscape(const uint32_t nsid, const size_t nBytes, std::span<std::byte> &outRange){
    struct EscapeHeader {
        uint32_t cmd;
        uint32_t nsid;
        uint32_t size;
    } __attribute__((__packed__));

    // allocate the buffer
    std::span<std::byte> range;
    int err = this->reserve(nBytes + sizeof(EscapeHeader), range);
    if(err) return err;

    // populate the buffer
    auto escHdr = reinterpret_cast<EscapeHeader *>(range.data());
    escHdr->cmd = SVGA_CMD_ESCAPE;
    escHdr->nsid = nsid;
    escHdr->size = nBytes;

    outRange = range.subspan(sizeof(*escHdr));
    return 0;
}

/**
 * Commits a sequence of bytes that's been written into the buffer provided by `reserve()` for
 * command submission.
 *
 * @param nBytes Number of command bytes to commit; may be less than reserved.
 */
int FIFO::commit(const size_t nBytes) {
    // get the current FIFO state
    volatile uint32_t *fifo = this->fifo;
    uint32_t max = fifo[SVGA_FIFO_MAX];
    uint32_t min = fifo[SVGA_FIFO_MIN];
    uint32_t nextCmd = fifo[SVGA_FIFO_NEXT_CMD];
    bool reserveable = this->hasCapability(SVGA_FIFO_CAP_RESERVE);

    if(kLogCommits) Trace("Committing %lu bytes of command data", nBytes);

    // clear reservation
    if(!this->reservedSize) {
        return Err::NoCommandsAvailable;
    }
    this->reservedSize = 0;

    // copy out of the bounce buffer if needed
    if(this->usingBounceBuffer) {
        auto buffer = this->bounceBuf.data();

        // copy as two chunks
        if(reserveable) {
            const auto chunkSz = std::min(nBytes, static_cast<size_t>(max - nextCmd));
            fifo[SVGA_FIFO_RESERVED] = nBytes;

            memcpy(nextCmd + reinterpret_cast<std::byte *>(this->fifo), buffer, chunkSz);
            memcpy(min + reinterpret_cast<std::byte *>(this->fifo), buffer + chunkSz,
                    nBytes - chunkSz);
        }
        // copy a dword at a time
        else {
            auto dword = reinterpret_cast<uint32_t *>(buffer);

            auto bytes = nBytes;
            while(bytes > 0) {
                fifo[nextCmd / sizeof(uint32_t)] = *dword++;
                nextCmd += sizeof(uint32_t);

                if(nextCmd == max) {
                    nextCmd = min;
                }

                fifo[SVGA_FIFO_NEXT_CMD] = nextCmd;
                bytes -= sizeof(uint32_t);
            }
        }
    }

    // update the NEXT_CMD field
    if(!this->usingBounceBuffer || reserveable) {
        nextCmd += nBytes;
        if(nextCmd >= max) {
            nextCmd -= (max - min);
        }
        fifo[SVGA_FIFO_NEXT_CMD] = nextCmd;
    }

    // clear FIFO reservation
    if(reserveable) {
        fifo[SVGA_FIFO_RESERVED] = 0;
    }

    return 0;
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



/**
 * Allocates a new fence and inserts it at the next avaialble position in the device's command
 * FIFO. The identifiers assigned to fences are monotonically increasing integers; though callers
 * should treat them as opaque tokens: we only guarantee that the token is never zero.
 *
 * @param outFence Opaque identifier for this fence
 *
 * @return 0 on success, error code otherwise.
 */
int FIFO::insertFence(uint32_t &outFence) {
    int err;
    std::span<std::byte> buffer;

    struct Fence {
        uint32_t cmd;
        uint32_t fenceId;
    } __attribute__((__packed__));

    // bail if FIFO doesn't support fences
    if(!this->hasCapability(SVGA_FIFO_CAP_FENCE)) {
        outFence = kUnsupportedFence;
        return 0;
    }

    // allocate fence number
    if(!this->nextFence) {
        this->nextFence = 1;
    }
    const auto fence = this->nextFence++;

    // allocate and build the command structure
    err = this->reserve(sizeof(Fence), buffer);
    if(err) return err;

    auto cmd = reinterpret_cast<Fence *>(buffer.data());
    cmd->cmd = SVGA_CMD_FENCE;
    cmd->fenceId = fence;

    // submit the command
    err = this->commitAll();
    if(err) return err;

    if(kLogFences) Trace("Allocated fence $%08x", fence);

    outFence = fence;
    return 0;
}

/**
 * Waits for the device to finish processing all commands preceding the given fence.
 *
 * TODO: Use interrupt driven mechanism
 *
 * @param fence Fence identifier (from `insertFence()`) to sync against
 *
 * @return 0 on success or error code
 */
int FIFO::syncToFence(const uint32_t fence) {
    // bail if invalid fence
    if(!fence || fence == kUnsupportedFence) return 0;

    // use legacy sync mechanism if hardware doesn't support fences
    if(!this->hasCapability(SVGA_FIFO_CAP_FENCE)) {
        this->s->regWrite(SVGA_REG_SYNC, 1);
        while(!this->s->regRead(SVGA_REG_BUSY)) {}
        return 0;
    }
    // bail if fence has already passed
    if(this->hasFencePassed(fence)) {
        return 0;
    }

    // wake up the host and spin until no longer busy
    bool busy{false};
    this->s->regWrite(SVGA_REG_SYNC, 1);

    while(!this->hasFencePassed(fence) && busy) {
        busy = !!this->s->regRead(SVGA_REG_BUSY);
    }

    return 0;
}

/**
 * Checks if we've passed the given fence.
 *
 * @note This does not handle wrap-around (where we have 2^31-1 fences generated) well. You should
 * discard a fence object once this call returns true.
 *
 * @param fence Fence identifier to checkpoint
 *
 * @return Whether the fence has passed.
 */
bool FIFO::hasFencePassed(const uint32_t fence) {
    // invalid fence
    if(!fence || fence == kUnsupportedFence) {
        return true;
    }
    else if(!this->hasCapability(SVGA_FIFO_CAP_FENCE)) {
        return false;
    }

    return (static_cast<int32_t>(this->fifo[SVGA_FIFO_FENCE] - fence)) >= 0;
}

/**
 * Wakes up the host to process commands.
 */
void FIFO::ringDoorbell() {
    if(this->isRegisterValid(SVGA_FIFO_BUSY) && !this->fifo[SVGA_FIFO_BUSY]) {
        this->fifo[SVGA_FIFO_BUSY] = true;

        this->s->regWrite(SVGA_REG_SYNC, 1);
    }
}
