#ifndef KERNEL_RUNTIME_QUEUE_H
#define KERNEL_RUNTIME_QUEUE_H

#include <stddef.h>

#include <mem/Heap.h>

#include <log.h>
#include <string.h>
#include <new>

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
            memset(this->cache, 0, sizeof(ListElement) * kElementCacheSize);
            this->prefillCache();
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

            // clear elements from cache
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(!this->cache[i]) continue;
                delete this->cache[i];
            }
        }

        /**
         * Inserts an element at the end of the list.
         */
        void push(const T &value) {
            ListElement *element = nullptr;

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

            if(!this->head) {
                this->head = element;
            }
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

            // return element to cache
            element->next = element->prev = nullptr;
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(this->cache[i]) continue;

                this->cache[i] = element;
                return value;
            }

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
            size_t numAllocated = 0;
            for(size_t i = 0; i < kElementCacheSize; i++) {
                if(this->cache[i]) continue;
                this->cache[i] = new ListElement;

                // ensure we don't allocate more than the max requested
                if(++numAllocated == max) return;
            }
        }

    private:
        /// head of the queue
        ListElement *head = nullptr;
        /// end of the queue
        ListElement *tail = nullptr;

        /// total number of items in the queue
        size_t numElements = 0;

        /// unused list elements
        ListElement *cache[kElementCacheSize];
};
}

#endif
