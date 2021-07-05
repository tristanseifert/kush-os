#ifndef LIBDRIVER_DMA_BUFFERPOOL_H
#define LIBDRIVER_DMA_BUFFERPOOL_H

#include <compare>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>

#include <driver/DmaBuffer.h>

namespace libdriver {
/**
 * Represents a contiguous region of virtual memory, with a particular maximum size, from which
 * smaller buffers can be allocated for IO.
 */
class BufferPool {
    public:
        /**
         * Represents a buffer that was created from this pool.
         */
        class Buffer: public DmaBuffer {
            friend class BufferPool;

            public:
                ~Buffer();

                /// Gets the size of this buffer
                size_t getSize() const override {
                    return this->size;
                }
                /// Return the physical pages that make up this buffer
                const std::vector<Extent> &getExtents() const override {
                    return this->extents;
                }
                /// Gets pointer to this buffer's data
                void * _Nonnull data() const override {
                    return reinterpret_cast<void *>(
                            reinterpret_cast<uintptr_t>(this->parent->base) + this->offset);
                }

                /// Gets the offset into the buffer pool.
                constexpr uintptr_t getPoolOffset() const {
                    return this->offset;
                }

            private:
                Buffer(BufferPool * _Nonnull pool, const uintptr_t offset, const size_t length);

            private:
                int status{0};

                /// Pool from which the buffer was allocated
                BufferPool * _Nonnull parent;
                /// Size of this buffer's allocation
                size_t size;
                /// Offset into the buffer pool's allocation region
                uintptr_t offset;

                std::vector<Extent> extents;
        };

    public:
        ~BufferPool();

        /// Attempts to allocate a sub-region of the buffer pool.
        [[nodiscard]] int getBuffer(const size_t length, std::shared_ptr<Buffer> &outBuffer);

        /// Return the handle of the underlying VM object.
        constexpr auto getHandle() const {
            return this->vmHandle;
        }
        /// Return the maximum size of the buffer pool.
        constexpr auto getMaxSize() const {
            return this->maxSize;
        }
        /// Return the currently allocated size of the buffer pool.
        constexpr auto getSize() const {
            return this->allocatedSize;
        }

        /// Allocate a new buffer pool.
        [[nodiscard]] static int Alloc(const size_t initial, const size_t maxSize,
                std::shared_ptr<BufferPool> &outPtr);

    private:
        /// Represents a free region of the buffer.
        struct FreeRegion {
            /// Offset from the start of the region
            uintptr_t offset;
            /// Length of the free region
            size_t length;

            inline auto operator<=>(const FreeRegion &r) const {
                return this->offset <=> r.offset;
            }
        };

    private:
        BufferPool(const size_t initial, const size_t maxSize);

        void freeRange(const uintptr_t offset, const size_t size);
        void defragFreeList();

    private:
        /// Status code used to abort initialization if needed
        int status{0};

        /**
         * Maximum size to which the buffer pool can grow. We reserve this entire size in the
         * virtual memory space to begin with, but only allocate a small subset of it.
         */
        size_t maxSize{0};
        /// Actual number of bytes allocated
        size_t allocatedSize{0};

        /// VM handle for this region
        uintptr_t vmHandle{0};
        /// Base of the mapping
        void * _Nonnull base;

        /// All free regions
        std::list<FreeRegion> freeList;
};
};

#endif
