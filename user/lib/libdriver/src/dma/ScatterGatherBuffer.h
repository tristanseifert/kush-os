#ifndef LIBDRIVER_DMA_SCATTERGATHERBUFFER_H
#define LIBDRIVER_DMA_SCATTERGATHERBUFFER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <driver/DmaBuffer.h>

namespace libdriver {
/**
 * Encapsulates a buffer in physical memory (locked) that can be used to perform scatter-gather
 * type DMA transfers. The buffer will always be page aligned, and allocate whole pages of memory.
 */
class ScatterGatherBuffer: public DmaBuffer {
    public:
        ~ScatterGatherBuffer();

        /// Gets a span that encompasses the entire buffer.
        explicit operator std::span<std::byte>() const override {
            return std::span<std::byte>(static_cast<std::byte *>(this->base), this->size);
        }

        /// Gets the status of the buffer; 0 is valid, any other value indicates an error
        constexpr const bool getStatus() const {
            return this->err;
        }

        /// Gets the pages that make up the buffer.
        const std::vector<Extent> &getExtents() const override {
            return this->extents;
        }
        /// Gets the size of the buffer
        size_t getSize() const override {
            return this->size;
        }

        /// Return the underlying virtual memory of the buffer.
        void * _Nonnull data() const override {
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
