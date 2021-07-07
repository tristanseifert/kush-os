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
    public:
        /// Maximum supported command size
        constexpr static const size_t kMaxCommandSize{1024 * 16};

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
};
} // namespace svga
