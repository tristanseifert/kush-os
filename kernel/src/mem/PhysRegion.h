#ifndef KERNEL_MEM_PHYSREGION_H
#define KERNEL_MEM_PHYSREGION_H

#include <stddef.h>
#include <stdint.h>

namespace mem {
/**
 * Encapsulates a single contiguous physical region of memory, from which page granularity
 * allocations can be made.
 *
 * This is implemented as a buddy system, with a configurable maximum order.
 *
 * TODO: add locking and stuff to make this work on multiprocessor systems
 */
class PhysRegion {
    public:
        /**
         * Maximum page order to use in the allocator
         *
         * The order is defined as a power-of-two multiplier on the page size: so, an allocation of
         * order 0 is one page, one of order 2 is 4 pages, and so forth (2^n * pageSize)
         *
         * This is, in other words, the size of the largest allocation we can service.
         */
        constexpr static const size_t kMaxOrder = 11;

    public:
        /// Can we create a region from the given range?
        static bool CanAllocate(const uint64_t base, const uint64_t len);
        /// Create a new physical region
        PhysRegion(const uint64_t base, const uint64_t len);
        /// VM system is available, map all structures.
        uintptr_t vmAvailable(uintptr_t base, const size_t len);

        /// Attempt to allocate the given number of contiguous physical pages.
        uint64_t alloc(const size_t numPages);
        /// Allocates a region of physical pages in the given order
        uint64_t allocOrder(const size_t order);

    private:
        /// Base address of physical memory identity mapping zone during early boot
        constexpr static uintptr_t kEarlyPhysIdentityMap = 0x0000000000000000;
        /// Base address of the physical memory identity mapping zone
        constexpr static uintptr_t kPhysIdentityMap = 0xffff800000000000;

        /**
         * Number of extra blocks to allocate space for during initialization.
         *
         * This sets a definite upper bound on the number of physical allocations that can be
         * satisfied before the kernel is fully booted with the VM subsystem enabled. By pre-
         * allocating these structs, we avoid the need for heap operations until later.
         */
        constexpr static size_t kInitialBlocks = 420;

        /**
         * Defines a single free block in an order.
         *
         * These blocks implicitly form a singly linked list.
         *
         * @note Addresses are relative to the start of the physical region.
         */
        struct Block {
            // Combined block address and flags (in the low 12 bits) value
            uint64_t value = 0;
            /// next block in the free list, or `nullptr` if at end
            Block *_Nullable next = nullptr;

            /// Returns the order of the block.
            inline const size_t getOrder() const {
                return (this->value & 0xF);
            }
            /// Sets the order of the block.
            inline void setOrder(const size_t order) {
                auto temp = this->value & ~0xF;
                temp |= order & 0xF;
                this->value = temp;
            }

            /// Returns the base address of the block.
            inline const uintptr_t getAddress() const {
                return (this->value & ~0xFFF);
            }
            /// Sets the base address of the block.
            inline void setAddress(const uintptr_t address) {
                auto temp = this->value & 0xFFF;
                temp |= (address & ~0xFFF);
                this->value = temp;
            }
        };

        /**
         * Information structure for a single order of pages. This is a power-of-two sized region
         * that maintains both a linked list of free pages, and a bitmap of the state of each of
         * the buddies of a block.
         */
        struct Order {
            /// First free block, if any.
            Block * _Nullable free = nullptr;

            /**
             * Block status bitmap
             *
             * Each block corresponds to a bit, which is set if allocated, clear if free.
             */
            uint8_t * _Nullable bitmap = nullptr;
            size_t bitmapSize = 0;

            /// order number
            size_t order = 0;
            /// number of allocatable blocks
            size_t numBlocks = 0;

            /// Gets the size, in bytes, of the given order.
            static size_t size(const size_t order);
            /// Applies fixups to virtual addresses, if needed; return number of bytes of VM used
            uintptr_t vmAvailable(PhysRegion &region, const uintptr_t blockVmBase,
                    const uintptr_t bitmapVmBase, const uintptr_t oldSlabBase);

            /// Allocates the first free block, and returns its address.
            Block * _Nullable allocBlock();
            /// Adds a block to the free list.
            void freeBlock(Block * _Nonnull block, const bool coalesce = false);

            /// Allocate and split a block. Return one half, the other is inserted into free list.
            Block * _Nullable split(PhysRegion &region, Block * _Nonnull,
                    const bool canGrow);

            private:
                /// Clears the allocation bit for a given block.
                inline void clearBit(Block * _Nonnull block) {
                    if(!this->bitmap) [[unlikely]] return;

                    const auto orderSz = Order::size(this->order);
                    const size_t bitIdx = block->getAddress() / orderSz;
                    this->bitmap[bitIdx / 8] &= ~(1 << (bitIdx % 8));
                }
                /// Sets the allocation bit for a given block.
                inline void setBit(Block * _Nonnull block) {
                    if(!this->bitmap) [[unlikely]] return;

                    const auto orderSz = Order::size(this->order);
                    const size_t bitIdx = block->getAddress() / orderSz;
                    this->bitmap[bitIdx / 8] |= (1 << (bitIdx % 8));
                }
                /// Tests the allocation bit for the given block and its buddy
                inline bool testBit(Block * _Nonnull block) {
                    // XXX: should this just panic?
                    if(!this->bitmap) [[unlikely]] return false;

                    const auto orderSz = Order::size(this->order);
                    const size_t bitIdx = block->getAddress() / orderSz;
                    return (this->bitmap[bitIdx / 8] & (1 << (bitIdx % 8))) != 0;
                }
        };

        /**
         * Header for a slab out of which blocks can be allocated. Slabs are linked together in a
         * list, should more slabs be allocated during runtime.
         *
         * A bitmap indicates which blocks are free to be allocated; a set bit indicates the block
         * is NOT allocated, while a clear bit indicates that it is allocated. (This is a small
         * optimization to allow for efficient checking of 64 blocks at a time.)
         */
        struct SlabHeader {
            /// Next slab
            SlabHeader *_Nullable next = nullptr;

            /// Base address (in VM space)
            uintptr_t vmBase = 0;
            /// Total length (including header) in bytes
            size_t length = 0;

            /// Offset relative to the start of the slab to the block allocation bitmap
            uintptr_t bitmapOff = 0;

            /// Total number of blocks in this slab
            size_t numBlocks = 0;
            /// Offset relative to the start of the slab to the block storage
            uintptr_t storageOff = 0;

            /// Create a new slab with the given extents.
            SlabHeader(const uintptr_t base, const size_t len);

            /// Allocate a new block
            Block * _Nullable alloc();
            /// Releases a previously allocated block
            bool free(Block * _Nonnull);

            /// Returns a pointer to the base of the storage array
            inline Block * _Nonnull getStorage() {
                return reinterpret_cast<Block *>(this->vmBase + this->storageOff);
            }
        };

    private:
        /// Allocates a block from our slab allocator.
        Block * _Nullable allocBlockStruct(const bool grow = true);
        /// Releases a block struct.
        void freeBlockStruct(Block * _Nonnull);

    private:
        /// whether initialization info is logged
        static bool gLogInit;
        /// When set, all page allocations are logged
        static bool gLogAlloc;
        /// whether block splits are logged
        static bool gLogSplits;
        /// whether VM pointer fixups are logged
        static bool gLogFixups;

    private:
        /// base address (physical) of this memory allocation region
        uint64_t base;
        /// size (in bytes) of this memory region
        uint64_t length;
        /// usable, allocatable storage (in bytes) inside this region
        uint64_t allocatable;
        /// number of allocatable pages (e.g. not used for accounting/metadata)
        size_t totalPages = 0;

        /// Start of the VM region reserved for this region's metadata
        uintptr_t vmBase = 0;
        /// Length of the VM region reserved for this region metadata
        size_t vmLength = 0;

        /// Holds data for each order of pages we can allocate.
        Order orders[kMaxOrder];

        /// offset to the first bitmap byte
        uintptr_t bitmapOff = 0;
        /// total bytes of bitmap data
        size_t bitmapSize = 0;

        /// first of the slabs from which block structs are allocated
        SlabHeader * _Nullable blockSlab = nullptr;
};
}

#endif
