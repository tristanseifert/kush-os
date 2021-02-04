#ifndef KERNEL_RUNTIME_VECTOR_H
#define KERNEL_RUNTIME_VECTOR_H

#include <stddef.h>

#include <mem/Heap.h>

#include <log.h>
#include <new>

namespace rt {
template<class T>
class Vector {
    private:
        /**
         * Internal iterator type
         */
        struct Iterator {
            friend class Vector<T>;

            private:
                /// Sets up the iterator. Nothing really to do here
                Iterator(Vector *_vec, const size_t index = 0) : i(index), vec(_vec) {}

            public:
                /// Nothing to do here, but we need to define it anyways
                ~Iterator() = default;

                /// Advance the iterator to the next position.
                Iterator &operator++() {
                    ++this->i;
                    return *this;
                }
                /// Returns the object we point to.
                T &operator*() {
                    return vec->storage[this->i];
                }
                /// Is this iterator at the same index as another?
                bool operator !=(const Iterator &it) {
                    return (it.i != this->i);
                }

            private:
                /// current index
                size_t i = 0;
                /// the vector we'll iterate
                Vector *vec = nullptr;
        };

    public:
        /**
         * Cleans up the vector's storage.
         */
        ~Vector() {
            for(size_t i = 0; i < this->numAllocated; i++) {
                this->storage[i].~T();
            }

            if(this->storage) {
                mem::Heap::free(this->storage);
            }
        }

        /**
         * Reserves space for the given number of items. If this is smaller than the number of
         * items currently in the vector, they are discarded.
         */
        void reserve(const size_t nItems) {
            const size_t newSz = nItems * sizeof(T);

            // truncation case
            if(nItems < this->numAllocated) {
                for(size_t i = nItems; i < this->numAllocated; i++) {
                    this->storage[i].~T();
                }

                this->numAllocated = nItems;
            }

            // resize the storage
            this->storage = reinterpret_cast<T *>(mem::Heap::realloc(this->storage, newSz));
            this->numReserved = nItems;
        }
        /**
         * Resizes the underlying storage.
         *
         * If the storage needs to grow, it will be resized to exactly `nItems`.
         */
        void resize(const size_t nItems) {
            // no change in size
            if(nItems == this->numAllocated) return;
            // truncation case
            else if(nItems < this->numAllocated) {
                for(size_t i = nItems; i < this->numAllocated; i++) {
                    this->storage[i].~T();
                }
            } 
            // otherwise, we're increasing the size
            else {
                if(nItems > this->numReserved) {
                    const size_t newSz = nItems * sizeof(T);
                    this->storage = reinterpret_cast<T *>(mem::Heap::realloc(this->storage, newSz));
                    this->numReserved = nItems;
                }

                // default construct the newly added items
                for(size_t i = this->numAllocated; i < nItems; i++) {
                    new(&this->storage[i]) T();
                }
            }

            // update allocation count
            this->numAllocated = nItems;
        }

        /**
         * Inserts a new item to the end of the vector.
         */
        void push_back(const T value) {
            // grow the buffer if needed
            if(!this->storage || this->numReserved == this->numAllocated) {
                // TODO: better growing algorithm
                const auto nItems = this->numReserved + 64;
                const size_t newSz = nItems * sizeof(T);

                this->storage = reinterpret_cast<T *>(mem::Heap::realloc(this->storage, newSz));
                this->numReserved = nItems;
            }

            // insert at the end
            this->storage[this->numAllocated++] = value;
        }
        /**
         * Removes all objects from the vector.
         *
         * This does not shrink the underlying memory.
         */
        void clear() {
            for(size_t i = 0; i < this->numAllocated; i++) {
                this->storage[i].~T();
            }

            this->numAllocated = 0;
        }

        /// Gets an iterator to the start of the vector
        Iterator begin() {
            return Iterator(this, 0);
        }
        /// Gets an iterator to the end of the vector
        Iterator end() {
            return Iterator(this, this->numAllocated);
        }

        /// Total number of objects stored in the vector
        const size_t size() const {
            return this->numAllocated;
        }
        /// Total number of objects our underlying storage has space for
        const size_t capacity() const {
            return this->numReserved;
        }
        /// Whether the vector is empty
        const bool empty() const {
            return !this->numAllocated;
        }

        /// Gets a reference to the given item
        const T& operator[](const size_t index) const {
            REQUIRE(index < this->numAllocated, "vector access out of bounds: %lu", index);
            return this->storage[index];
        }
        /// Gets a reference to the given item
        T& operator[](const size_t index) {
            REQUIRE(index < this->numAllocated, "vector access out of bounds: %lu", index);
            return this->storage[index];
        }

    private:
        /// number of allocated elements in the array
        size_t numAllocated = 0;
        /// number of reserved elements in the array
        size_t numReserved = 0;

        /// array contents
        T *storage = nullptr;
};
}

#endif
