#ifndef LIBDRIVER_DMA_SCATTERGATHERBUFFER_H
#define LIBDRIVER_DMA_SCATTERGATHERBUFFER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace libdriver {
/**
 * Encapsulates a buffer in physical memory (locked) that can be used to perform scatter-gather
 * type DMA transfers. The buffer will always be page aligned, and allocate whole pages of memory.
 */
class ScatterGatherBuffer {
    public:
        /**
         * Describes a single extent of the scatter/gather buffer, aka an individual contiguous
         * chunk of physical memory to be transfered.
         */
        struct Extent {
            friend class ScatterGatherBuffer;

            /// Physical address of the extent
            uint64_t physAddr{0};
            /// Length of the extent, in bytes
            size_t length{0};

            constexpr uint64_t getPhysAddress() const {
                return this->physAddr;
            }
            constexpr size_t getSize() const {
                return this->length;
            }

            Extent(const uint64_t addr, const size_t size) : physAddr(addr), length(size) {}
        };

        /// Scatter gather buffer errors
        enum Errors: int {
            /// Failed to translate physical address
            PhysTranslationFailed               = -20000,
            /// Allocating physical memory failed
            AllocFailed                         = -20001,
            /// Mapping the memory region failed
            MapFailed                           = -20002,
        };

    public:
        ~ScatterGatherBuffer();

        /// Gets a span that encompasses the entire buffer.
        explicit operator std::span<std::byte>() const {
            return std::span<std::byte>(static_cast<std::byte *>(this->base), this->size);
        }

        /// Gets the status of the buffer; 0 is valid, any other value indicates an error
        constexpr const bool getStatus() const {
            return this->err;
        }

        /// Gets the pages that make up the buffer.
        constexpr const auto &getExtents() const {
            return this->extents;
        }
        /// Gets the size of the buffer
        constexpr size_t getSize() const {
            return this->size;
        }

        /// Return the underlying virtual memory of the buffer.
        constexpr void * _Nonnull data() const {
            return this->base;
        }

        /// Allocate a new scatter gather buffer.
        [[nodiscard]] static int Alloc(const size_t size,
                std::shared_ptr<ScatterGatherBuffer> &outPtr);

    private:
        ScatterGatherBuffer(const size_t size);

    private:
        /// Current error code of the buffer
        int err{0};

        /// Length of the buffer in bytes
        size_t size{0};
        /// Handle to the VM object
        uintptr_t vmHandle{0};
        /// Base of the mapping
        void * _Nonnull base;

        std::vector<Extent> extents;
};
}

#endif
