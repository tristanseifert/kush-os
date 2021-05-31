#ifndef KERNEL_RUNTIME_LOCKFREEQUEUE_H
#define KERNEL_RUNTIME_LOCKFREEQUEUE_H

#include <cstddef>
#include <cstdint>
#include <new>

#include <bitflags.h>
#include <log.h>
#include <string.h>

#include "mem/Heap.h"

namespace rt {
/// Helper to determine if the input value is a power of 2
constexpr bool ispow2(size_t v) {
    return v && ((v & (v - 1)) == 0);
}

/// Flags for the push/pop calls
ENUM_FLAGS(LockFreeQueueFlags)
enum class LockFreeQueueFlags {
    /// No special treatment for the operation
    kNone                       = 0,

    /// The caller is the only producer thread
    kSingleProducer             = (1 << 0),
    /// The caller is willing to accept insertion of fewer elements than requested
    kPartialPush                = (1 << 1),

    /// The caller is the only consumer thread
    kSingleConsumer             = (1 << 8),
    /// The caller is willing to accept fewer popped items than requested
    kPartialPop                 = (1 << 9),
};


/**
 * Lock free queue supporting multiple producer, multiple consumer scenarios.
 *
 * Storage space for the queue elements is preallocated, so that insertion and dequeuing do not
 * perform any memory accesses and are implemented entirely with atomic operations. Additionally,
 * the size of the storage space must be a power of two. (You will only be able to actually insert
 * up to N-1 elements.) This optimization works best if the objects stored in the queue are also
 * a power of two size.
 */
template<class T, size_t kDefaultSize = 64>
class LockFreeQueue {
    static_assert(ispow2(kDefaultSize), "queue storage must be a power of 2");

    public:
        using Flags = LockFreeQueueFlags;

    public:
        /**
         * Creates a new queue with the given number of elements of storage space.
         *
         * @param size Number of elements to reserve storage space for.
         */
        LockFreeQueue(const size_t size = kDefaultSize) {
            this->resize(size);
        }

        /**
         * Release all storage space associated with the queue.
         *
         * @note This does NOT serialize on any waiting threads; you must be sure that there are no
         * other threads accessing the queue before deallocating it.
         */
        ~LockFreeQueue() {
            if(this->storage) {
                mem::Heap::free(this->storage);
            }
        }



        /**
         * Returns the allocated capacity of the queue.
         */
        inline size_t capacity() const {
            size_t temp;
            __atomic_load(&this->allocatedCapacity, &temp, __ATOMIC_RELAXED);
            return temp;
        }

        /**
         * Tests whether the queue is empty.
         */
        inline bool empty() const {
            return (this->size() == 0);
        }

        /**
         * Returns the number of elements in the queue.
         */
        inline size_t size() const {
            return (this->prodTail - this->consHead);
        }

        /**
         * Returns the number of free storage slots in the queue.
         */
        inline size_t capacityFree() const {
            const auto capacity = this->prodMask;
            const auto free = (capacity - this->consTail - this->prodHead);
            //return (free > this->prodMask) ? this->prodMask : free;
            return free;
        }



        /**
         * Resizes the internal storage of the queue.
         *
         * @note This method is NOT thread safe. There must be no other threads accessing the queue
         * when invoked.
         *
         * @param newSize New size of the queue. If there are more elements than the new size has
         *                space for, the elements at the tail end of the queue are discarded.
         */
        void resize(const size_t newSize) {
            // validate size
            REQUIRE(ispow2(newSize), "queue size must be power of 2");

            // TODO: realign all entries to start of buffer
            if(this->storage) {
                // XXX: instead we just reset the queue lmao
                this->prodTail = 0;
                this->prodHead = 0;
                this->consTail = 0;
                this->consHead = 0;
            }

            // reallocate buffer and update size
            const size_t newAllocSize = sizeof(T) * newSize;
            this->storage = static_cast<T *>(mem::Heap::realloc(this->storage, newAllocSize));

            this->allocatedCapacity = newSize;

            this->prodMask = (newSize - 1);
            this->consMask = (newSize - 1);
        }



        /**
         * Pushes a single item to the back of the queue.
         *
         * @return Number of items that were actually pushed to the queue
         */
        inline size_t insert(const T &data, const Flags flags = Flags::kNone) {
            return this->insert(&data, 1, flags & ~Flags::kPartialPush);
        }

        /**
         * Pushes an item to the back of the queue.
         *
         * We'll attempt to reserve the requested number of data slots, then copy the data, then
         * update the pointers to indicate data is available.
         *
         * @param inData Pointer to one or more objects to insert
         * @param count Number of elements in the data pointer to insert
         * @param flags Define whether we accept pushing partial items or whether we're the only
         *              producer.
         *
         * @return Number of items that were actually pushed to the queue
         */
        size_t insert(const T *inData, const size_t count, const Flags flags = Flags::kNone) {
            size_t oldProdHead, consTail, newProdHead;

            // get queue capacity
            size_t mask = this->prodMask;
            size_t capacity = mask;

            // set once we've reserved enough space for the requested number of items
            bool success = false;

            // read the producer head ptr
            oldProdHead = __atomic_load_n(&this->prodHead, __ATOMIC_RELAXED);

            // reserve enough slots for the data we want
            size_t freeEntries, n;
            do {
                // try to always push the total number of items
                n = count;
                __atomic_thread_fence(__ATOMIC_ACQUIRE);

                // load tail ptr (acquire to ensure other threads' writes are visible)
                consTail = __atomic_load_n(&this->consTail, __ATOMIC_ACQUIRE);
                freeEntries = (capacity + consTail - oldProdHead);

                // caller will accept partial push and we can't fit all items
                if((freeEntries < count) && TestFlags(flags & Flags::kPartialPush)) {
                    n = freeEntries;
                }

                // queue is full/insufficient space for all items and no partial pushing
                if(!freeEntries || !n || (freeEntries < n)) {
                    // we could provide the user the freeEntries value here
                    return 0;
                }

                newProdHead = oldProdHead + n;
                success = true;

                if(TestFlags(flags & Flags::kSingleProducer)) {
                    this->prodHead = newProdHead;
                } else {
                    success = __atomic_compare_exchange_n(&this->prodHead, &oldProdHead,
                            newProdHead, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                }
            } while(!success);

            // prepare to insert data
            REQUIRE(n <= capacity + __atomic_load_n(&this->consTail, __ATOMIC_RELAXED) - oldProdHead, "write past tail");
            REQUIRE(n > 0 && n <= count, "invalid %s reservation length", "write");
            REQUIRE(freeEntries >= n, "%s reservation exceeds free space", "write");

            // store it
            for(size_t i = 0; i < n; i++) {
                //memcpy(&this->storage[(oldProdHead + i) & mask], &inData[i], sizeof(T));
                //this->storage[(oldProdHead + i) & mask] = inData[i];
                new(&this->storage[(oldProdHead + i) & mask]) T(inData[i]);
            }

            // wait for any other threads to finish
            while(this->prodTail != oldProdHead) {
                __cpu_relax();
            }

            // update the tail ptr to indicate more data available.
            __atomic_store_n(&this->prodTail, newProdHead, __ATOMIC_RELEASE);

            return n;
        }


        /**
         * Pops a single item off the queue and writes it in the given storage space.
         *
         * @param out Storage space for the item
         * @param flags Whether the caller is the only consumer thread.
         *
         * @return Whether an item was successfully popped or not.
         */
        bool pop(T &out, const Flags flags = Flags::kNone) {
            T temp;
            size_t ret = this->pop(&temp, 1, flags & ~Flags::kPartialPop);

            if(ret) {
                out = temp;
                return true;
            }
            return false;
        }

        /**
         * Pops a given number of items off the queue.
         *
         * @param outData Pointer to storage space for up to `count` items
         * @param count Maximum number of items to retrieve; the output storage must fit this many
         * @param flags Whether the caller is the only consumer, and whether they are willing to
         *              accept less than the requested number of items read from the queue.
         *
         * @return Total number of items popped off the queue, or 0 if failure.
         */
        size_t pop(T *outData, const size_t count, const Flags flags = Flags::kNone) {
            size_t oldConsHead, prodTail, newConsHead;
            size_t mask = this->prodMask;

            // set once we've acquired the read rights to the given number of items
            bool success = false;

            // get consumer head ptr
            oldConsHead = __atomic_load_n(&this->consHead, __ATOMIC_RELAXED);

            // acquire read rights to up to `count` items
            size_t readyEntries, n;
            do {
                n = count;

                // ensure writes by other threads are visible
                __atomic_thread_fence(__ATOMIC_ACQUIRE);
                prodTail = __atomic_load_n(&this->prodTail, __ATOMIC_ACQUIRE);

                // this can overflow, but for unsigned types that's ok :D
                readyEntries = prodTail - oldConsHead;
                REQUIRE(readyEntries < (mask + 1), "number of free entries overflow (%d)", readyEntries);

                // the caller will accept fewer than `count` items
                if(readyEntries < count && TestFlags(flags & Flags::kPartialPop)) {
                    n = readyEntries;
                }
                // handle failure (no/insufficient items)
                if(!readyEntries || !n || readyEntries < n) {
                    return 0;
                }

                newConsHead = oldConsHead + n;

                // update head ptr
                success = true;
                if(TestFlags(flags & Flags::kSingleConsumer)) {
                    this->consHead = newConsHead;
                } else {
                    success = __atomic_compare_exchange_n(&this->consHead, &oldConsHead,
                            newConsHead, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                }
            } while(!success);

            // prepare to read items out
            REQUIRE(n <= __atomic_load_n(&this->prodTail, __ATOMIC_RELAXED) - oldConsHead, "read past tail");
            REQUIRE(n > 0 && n <= count, "invalid %s reservation length", "read");
            REQUIRE(readyEntries >= n, "%s reservation exceeds free space", "read");

            // copy data out
            for(size_t i = 0; i < n; i++) {
                //memcpy(&outData[i], &this->storage[(oldConsHead + i) & mask], sizeof(T));
                outData[i] = this->storage[(oldConsHead + i) & mask];
                this->storage[(oldConsHead + i) & mask].~T(); 
            }

            // update tail ptr after all other threads are done
            while(this->consTail != oldConsHead) {
                __cpu_relax();
            }

            // update tail ptr
            __atomic_store_n(&this->consTail, newConsHead, __ATOMIC_RELEASE);

            return n;
        }

    private:
        /**
         * This function indicates to the processor that we're in a busy loop; it needs likely to
         * be defined per architecture.
         */
        __attribute__((always_inline)) static inline void __cpu_relax() {
#if defined(__i386__) || defined(__amd64__)
            do { asm volatile("pause\n": : :"memory"); } while(0);
#else
#error Provide implementation of `__cpu_relax` in LockFreeQueue for current arch
#endif
        }


    private:
        /**
         * Producer (for enqueue operation) head and tail pointers; on enqueue, the head is moved
         * forward, while the tail is updated by consumers
         *
         * The producer tail points to the first empty slot.
         */
        volatile size_t prodHead = 0, prodTail = 0;
        /**
         * Bitmask for the size of the queue; bit operations are faster than division when it comes
         * to bounds checking. This is always (allocatedCapacity - 1), so for a queue of size 8,
         * this field contains 0b0111.
         */
        size_t prodMask = 0;
        // padding to 64-byte (cache line) align to avoid false sharing
        uint8_t _padding1[64-(sizeof(size_t) * 3)];

        /**
         * Consumer (for pop operation) head and tail pointers; on pop, the tail is moved forward,
         * and writers look at the head (which is also advanced) to find a new free slot.
         */
        volatile size_t consHead = 0, consTail = 0;
        /// bitmask for consumer size (repeated to avoid false sharing)
        size_t consMask = 0;
        // padding to 64-byte (cache line) align to avoid false sharing
        uint8_t _padding2[64-(sizeof(size_t) * 3)];

        /// content storage
        T *storage = nullptr;
        /// Total number of elements of space reserved in the queue
        size_t allocatedCapacity;
};

// TODO: it would be nice to check for cache alignment but not really possible with templates?
}

#endif
