#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <libpci/Device.h>

class SVGA;

namespace svga {
/**
 * Handles the command FIFO on the SVGA device, including initialization of it and managing
 * commands inside it.
 */
class FIFO {
    friend class Commands2D;

    public:
        /// Maximum supported command size
        constexpr static const size_t kMaxCommandSize{1024 * 16};

        /// Fence value assigned if the hardware doesn't support synchronization
        constexpr static const uint32_t kUnsupportedFence{UINT32_MAX};

    public:
        /// Initializes a FIFO belonging to the given SVGA controller
        FIFO(SVGA *controller, const libpci::Device::AddressResource &bar);
        /// Releases FIFO memory.
        ~FIFO();

        /// Check whether a particular FIFO capability bit is set.
        [[nodiscard]] bool hasCapability(const uintptr_t cap) const;
        /// Check whether space has been allocated for the provided register
        [[nodiscard]] bool isRegisterValid(const uintptr_t reg) const;

        /// Reserve memory for a command in the FIFO.
        [[nodiscard]] int reserve(const size_t bytes, std::span<std::byte> &outRange);
        /// Reserve memory for a command in the FIFO with a single dword "type" field prefix.
        [[nodiscard]] int reserveCommand(const uint32_t type, const size_t nBytes,
                std::span<std::byte> &outRange);
        /// Reserves memory for an escape command.
        [[nodiscard]] int reserveEscape(const uint32_t nsid, const size_t nBytes,
                std::span<std::byte> &outRange);

        /// Commits the given number of bytes written to the FIFO
        [[nodiscard]] int commit(const size_t nBytes);
        /// Commits the entire byte range of the command currently built up in the FIFO.
        [[nodiscard]] int commitAll() {
            return this->commit(this->reservedSize);
        }

        /// Allocate a new sync fence in the FIFO.
        [[nodiscard]] int insertFence(uint32_t &outFence);
        /// Synchronizes execution to the given fence.
        [[nodiscard]] int syncToFence(const uint32_t fence);
        /// Have we processed all FIFO commands prior to the given fence?
        [[nodiscard]] bool hasFencePassed(const uint32_t fence);

        /// Gets size of FIFO, in bytes
        constexpr inline auto getSize() const {
            return this->size;
        }
        /// Get initialization status
        constexpr inline auto getStatus() const {
            return this->status;
        }

    private:
        void handleFifoFull();

        /// Notifies the host we've more commands available.
        void ringDoorbell();

    private:
        /// Whether capability tests are logged
        constexpr static const bool kLogCapabilities{false};
        /// Whether command reservations are logged
        constexpr static const bool kLogReservations{false};
        /// Whether command commits are logged
        constexpr static const bool kLogCommits{false};
        /// Whether fences are logged
        constexpr static const bool kLogFences{false};

    private:
        SVGA *s;

        /// initialization status
        int status{0};

        /// VM handle for the FIFO region
        uintptr_t vmRegion{0};
        /// Base address of the FIFO region
        uint32_t *fifo{nullptr};
        /// Number of bytes of total FIFO space (including space reserved for registers)
        size_t size{0};
        /// Buffer for noncontiguous FIFO commands
        std::vector<std::byte> bounceBuf;

        /// Whether the FIFO bounce buffer is currently in use
        bool usingBounceBuffer{false};
        /// Total reserved bytes of FIFO memory
        size_t reservedSize{0};

        /// Value to use for the next sync fence
        uint32_t nextFence{1};
};
} // namespace svga
