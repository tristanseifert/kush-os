#ifndef KERNEL_RUNTIME_QUEUE_H
#define KERNEL_RUNTIME_QUEUE_H

#include "List.h"

#include <stddef.h>
#include <log.h>

namespace rt {
/**
 * A standard FIFO queue; objects can be pushed into it at the back, and popped off the front. We
 * implement this as an adapter on top of the existing linked list class, since it provides O(1)
 * access to the head and tail of its storage.
 *
 * TODO: Probably a good idea to use slab alloc for linked list elements... they're kind of a pain
 * in the ass and we shouldn't pay for a full trip through the memory allocator for every single
 * insertion/removal. We implement a small cache for them though.
 */
template<class T>
class Queue {
    public:
        /**
         * Inserts an element at the end of the list.
         */
        void push_back(const T &value) {
            this->storage.append(value);
        }
        /**
         * Inserts an element at the front of the queue.
         */
        void push_front(const T &value) {
            this->storage.prepend(value);
        }

        /**
         * Pops an element off of the front of the queue.
         */
        T pop() {
            // we must have an element
            REQUIRE(!this->empty(), "cannot pop empty queue");

            // get the value from the list and remove it
            auto value = this->storage[0];
            this->storage.remove(0);

            // return it
            return value;
        }

        /**
         * Peeks at the front of the queue; it is not removed.
         */
        const T *peek() const {
            if(this->empty()) return nullptr;
            return &this->storage[0];
        }

        /// Is the queue empty?
        const bool empty() const {
            return this->storage.empty();
        }
        /// Number of items in the queue
        const size_t size() const {
            return this->storage.size();
        }

        /// Gets a reference to the underlying list
        auto &getStorage() {
            return this->storage;
        }

    private:
        /// storage of elements
        List<T> storage;
};
}

#endif
