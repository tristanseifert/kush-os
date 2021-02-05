#ifndef KERNEL_MEM_SLABALLOCATOR_H
#define KERNEL_MEM_SLABALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include <arch.h>
#include <arch/spinlock.h>
#include <log.h>
#include <mem/AnonPool.h>

namespace mem {
/**
 * Slab allocators allocate virtual memory in chunks, called slabs, each containing a fixed number
 * of objects of the given type. Slabs are automatically allocated and released as required.
 *
 * We allocate slabs from the kernel's anonymous VM pool. Slabs are linked together as a doubly-
 * linked list, so we can traverse them in either order.
 */
template<class T, size_t slabSz = (32 * 1024)>
class SlabAllocator {
    public:
        /**
         * Allocates the first slab and initializes the allocator structures.
         */
        SlabAllocator() {
            // allocate slab
            this->head = this->allocSlab();
        }

        /**
         * Releases all memory we allocated.
         */
        ~SlabAllocator() {
            // remove all slabs
            auto slab = this->head;
            while(slab) {
                auto next = slab->next;
                this->freeSlab(slab);

                slab = next;
            }
        }

        /**
         * Allocates a new object. We'll check each slab to see if it has available space, and if
         * none do, allocate a new slab.
         */
        template<typename... Args>
        T *alloc(Args... arg) {
            // iterate over all slabs to find one that has space
            auto slab = this->head;
            while(slab) {
                // make allocation if space available
                if(!slab->full()) {
                    auto ptr = slab->alloc(arg...);
                    if(ptr) return ptr;
                }

                // try next one
                slab = slab->next;
            }

            // if we get here, all slabs are full. allocate a new one
            auto slabule = this->allocSlab();
            return slabule->alloc(arg...);
        }

        /**
         * Releases a previously allocated object.
         *
         * Should the slab containing the object be completely empty, we'll de-allocate it.
         */
        void free(T *ptr) {
            // iterate over all slabs to find which one this belongs to
            auto slab = this->head;
            while(slab) {
                if(slab->contains(ptr)) {
                    return slab->free(ptr);
                }
            }

            // if we get here, no slab contained the object
            panic("slab alloc %p failed to find object %p", this, ptr);
        }

    private:
        /**
         * Slab data structure; it holds metadata _and_ the actual object storage.
         */
        struct Slab {
            /// total number of items we've storage for in the slab
            constexpr static const size_t kNumItems = (slabSz - 24 - (slabSz / sizeof(T) / 8)) / sizeof(T);

            /// pointer to the previous slab
            Slab *prev = nullptr;
            /// pointer to the next slab
            Slab *next = nullptr;

            /// number of allocated objects (this way, empty()/full() needn't iterate the bitmap)
            size_t numAllocated = 0;

            /// allocation bitmap: 1 = free, 0 = allocated
            uint32_t freeMap[(kNumItems + 32 - 1) / 32];
            // storage for the objects
            uint8_t storage[sizeof(T) * kNumItems];

            /**
             * Marks all elements as allocated when the slab is allocated.
             */
            Slab() {
                for(size_t i = 0; i < kNumItems/32; i++) {
                    this->freeMap[i] = 0xFFFFFFFF;
                }
                if(kNumItems % 32) {
                    this->freeMap[kNumItems / 32] = 0;

                    const auto remainder = kNumItems % 32;
                    for(size_t i = 0; i < remainder; i++) {
                        this->freeMap[kNumItems / 32] |= (1 << i);
                    }
                } 
            }

            /**
             * Invoke destructors of allocated objects.
             */
            ~Slab() {
                for(size_t i = 0; i < kNumItems; i++) {
                    if((this->freeMap[i / 32] & (1 << (i % 32))) == 0) {
                        auto item = reinterpret_cast<T *>(&this->storage[i * sizeof(T)]);
                        item->~T();
                    }
                }
            }

            /**
             * Allocates a new object from this slab, if we've got available space.
             */
            template<typename... Args>
            T *alloc(Args... ctorArgs) {
                const auto elements = (kNumItems + 32 - 1) / 32;
                for(size_t i = 0; i < elements; i++) {
                    // if no free pages in this element, check next
                    if(!this->freeMap[i]) continue;
                    // get index of the first free (set) bit; ensure it's not out of bounds
                    const size_t allocBit = __builtin_ffs(this->freeMap[i]) - 1;
                    const auto off = (i * 32) + allocBit;
                    if(off >= kNumItems) return nullptr;

                    // mark as allocated and set up the object
                    this->freeMap[i] &= ~(1 << allocBit);
                    this->numAllocated++;

                    auto item = reinterpret_cast<T *>(&this->storage[off * sizeof(T)]);
                    memset(item, 0, sizeof(T));
                    new(item) T(ctorArgs...);

                    return item;
                }

                // no free memory
                return nullptr;
            }

            /**
             * Releases an object previously allocated from this slab.
             *
             * @note The pointer MUST be an object inside the slab. Behavior otherwise is
             * undefined.
             */
            void free(T *ptr) {
                // convert the pointer into an object offset
                const auto off = (((uintptr_t) ptr) - ((uintptr_t) &this->storage)) / sizeof(T);

                // ensure it's allocated
                REQUIRE(!(this->freeMap[off/32] & (1 << (off % 32))),
                        "slab %p ptr %p not allocated!", this, ptr);
                this->numAllocated--;

                // invoke its destructor
                auto item = reinterpret_cast<T *>(&this->storage[off * sizeof(T)]);
                item->~T();

                // mark as free
                this->freeMap[off / 32] |= (1 << (off % 32));
            }

            /**
             * Determines whether the given allocation came from this slab.
             */
            const bool contains(T *ptr) {
                auto addr = ((uintptr_t) ptr);
                const auto base = (uintptr_t) &this->storage;

                // may not be before the storage array
                if(addr < base) return false;
                // also can't be more than the storage array size
                if(addr > (base + (sizeof(T) * kNumItems))) return false;

                // if we get here, we must contain the object
                return true;
            }

            /**
             * Determines whether this slab is fully occupied.
             */
            const bool full() const {
                return (this->numAllocated == kNumItems);
            }
            /**
             * Determines whether there are any allocations in the slab.
             */
            const bool empty() const {
                return !this->numAllocated;
            }
        };
        static_assert(sizeof(Slab) <= slabSz, "Slab too large");

    private:
        /**
         * Allocates a new slab in kernel anonymous virtual memory region. The returned pointer is
         * to a fully initialized slab object, with no allocated objects.
         */
        Slab *allocSlab() {
            // get the memory pages
            const auto pageSz = arch_page_size();
            const auto slabPages = (slabSz + pageSz - 1) / pageSz;
            void *base = mem::AnonPool::allocPages(slabPages);

            // initialize the slab
            auto slab = reinterpret_cast<Slab *>(base);
            new(slab) Slab();

            // update list linkages
            if(this->tail) {
                this->tail->next = slab;
                slab->prev = this->tail;
            }

            this->tail = slab;

            // done
            return slab;
        }

        /**
         * Releases the virtual memory held by the given slab.
         */
        void freeSlab(Slab *slab) {
            // invoke the slab destructor
            slab->~Slab();

            // return memory back to the kernel
            const auto pageSz = arch_page_size();
            const auto slabPages = (slabSz + pageSz - 1) / pageSz;
            mem::AnonPool::freePages(slab, slabPages);
        }

    private:
        /// first slab
        Slab *head = nullptr;
        /// last slab (end of the linked list of slabs)
        Slab *tail = nullptr;
};
}

#endif
