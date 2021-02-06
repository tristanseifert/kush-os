#ifndef KERNEL_RUNTIME_QUEUE_H
#define KERNEL_RUNTIME_QUEUE_H

#include <stddef.h>

#include <mem/Heap.h>

#include <log.h>
#include <string.h>
#include <new>

// whether the queue caches pointers to list elements
#define QUEUE_WITH_CACHE                0

namespace rt {
/**
 * A standard FIFO queue; objects can be pushed into it at the back, and popped off the front. Its
 * data is stored as a linked list. This allows O(1) access and insertion, but random access is
 * not supported.
 *
 * TODO: Probably a good idea to use slab alloc for linked list elements... they're kind of a pain
 * in the ass and we shouldn't pay for a full trip through the memory allocator for every single
 * insertion/removal. We implement a small cache for them though.
 */
template<class T, size_t kElementCacheSize = 16>
class Queue {
    static_assert(kElementCacheSize > 2, "invalid element cache size");

    private:
        /// Linked list elements used to maintain the queue
        struct ListElement {
            T value;
            ListElement *prev, *next;
        };

    public:
        /**
         * Initializes the new queue. We'll pre-allocate some list elements here.
         */
        Queue() {
#if QUEUE_WITH_CACHE
            memset(this->cache, 0, sizeof(ListElement) * kElementCacheSize);
            this->prefillCache();
#endif
        }

        /**
         * Cleans up all list elements.
         */
        ~Queue() {
            // walk the list and delete them all
            auto head = this->head;
            while(head) {
                auto next = head->next;
                delete head;
                head = next;
            }

#if QUEUE_WITH_CACHE
            // clear elements from cache
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(!this->cache[i]) continue;
                delete this->cache[i];
            }
#endif
        }

        /**
         * Inserts an element at the end of the list.
         */
        void push(const T &value) {
            ListElement *element = nullptr;

#if QUEUE_WITH_CACHE
again:;
            // try to find a list element from the cache
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(this->cache[i]) {
                    element = this->cache[i];
                    this->cache[i] = nullptr;
                    goto beach;
                }
            }

            // none found; allocate some and try again
            this->prefillCache(kElementCacheSize / 2);
            goto again;

beach:;
#else
            element = new ListElement;
#endif
            REQUIRE(element, "failed to allocate element");

            // found an element, add it on the end of the list
            element->value = value;
            element->prev = this->tail;
            element->next = nullptr;

            if(this->tail) {
                this->tail->next = element;
            }

            // bookkeeping
            this->numElements++;
            this->tail = element;

            //log("adding element %p (%p, %p) count %lu %p", element, element->prev, element->next, this->numElements, value);

            if(!this->head) {
                this->head = element;
            }
            // log("state %p %p", this->head, this->tail);
        }

        /**
         * Pops an element off of the front of the queue.
         */
        T pop() {
            // we must have an element
            REQUIRE(this->head, "cannot pop empty queue");

            // get the list element and its value
            ListElement *element = this->head;
            const auto value = element->value;

            // update bookkeeping
            this->head = element->next;
            if(!this->head) {
                this->tail = nullptr;
            }

            this->numElements--;
            // log("state %p %p (%lu)", this->head, this->tail, this->numElements);

#if QUEUE_WITH_CACHE
            // return element to cache
            element->next = element->prev = nullptr;
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(this->cache[i]) continue;

                this->cache[i] = element;
                return value;
            }
#endif

            // if we get here, no space in cache
            delete element;
            return value;
        }

        /**
         * Peeks at the front of the queue; it is not removed.
         */
        const T *peek() const {
            if(!this->head) return nullptr;
            return &this->head->value;
        }

        /// Is the queue empty?
        const bool empty() const {
            return (this->head == nullptr);
        }
        /// Number of items in the queue
        const size_t size() const {
            return this->numElements;
        }

    private:
        /**
         * Fills all empty slots in the cache
         */
        void prefillCache(const size_t max = kElementCacheSize) {
#if QUEUE_WITH_CACHE
            size_t numAllocated = 0;
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(this->cache[i]) continue;
                this->cache[i] = new ListElement;

                // ensure we don't allocate more than the max requested
                if(++numAllocated == max) return;
            }
#endif
        }

    private:
        /// head of the queue
        ListElement *head = nullptr;
        /// end of the queue
        ListElement *tail = nullptr;

        /// total number of items in the queue
        size_t numElements = 0;

#if QUEUE_WITH_CACHE
        /// unused list elements
        ListElement *cache[kElementCacheSize];
#endif
};
}

#endif
