#ifndef KERNEL_RUNTIME_MINHEAP_H
#define KERNEL_RUNTIME_MINHEAP_H

#include <cstddef>
#include <cstdint>

#include <log.h>

#include "Vector.h"

namespace rt {
/**
 * Templated min-heap, which allows O(1) access at the value that compares lowest. Pushing and
 * popping elements is accomplished in O(log n) time.
 */
template<class T>
class MinHeap {
    public:
        /**
         * Returns the total amount of elements in the heap.
         */
        size_t size() const {
            return this->storage.size();
        }

        /**
         * Whether the heap is empty or not
         */
        bool empty() const {
            return this->storage.empty();
        }

        /**
         * Inserts a new item into the heap.
         */
        void insert(const T &object) {
            // insert at back of heap
            this->storage.push_back(object);

            // restore heapification
            const auto idx = this->size() - 1;
            this->heapifyUp(idx);
        }

        /**
         * Remove the minimum element.
         */
        void extract() {
            REQUIRE(!this->empty(), "cannot %s min item on empty heap", "pop");

            this->storage[0] = this->storage.back();
            this->storage.pop_back();

            this->heapifyDown(0);
        }

        /**
         * Returns the minimum element in the heap.
         */
        T& min() {
            REQUIRE(!this->empty(), "cannot %s min item on empty heap", "peek at");
            return this->storage.front();
        }
        /**
         * Returns the minimum element in the heap.
         */
        T& min() const {
            REQUIRE(!this->empty(), "cannot %s min item on empty heap", "peek at");
            return this->storage.front();
        }

        /**
         * Iterate over all items in the heap, with the option to remove them.
         *
         * @note Once an item has been removed, the structure of the heap changes and the
         * enumeration order becomes undefined.
         *
         * @param callback Method invoked for each object; return `true` from it to keep
         *                 enumerating, or `false` to stop.
         *                 Set the *remove param to `true` to remove that object.
         */
        void enumerateObjects(bool (*callback)(T &, bool *, void *), void *ctx) {
            size_t i = 0;
            while(i < this->size()) {
                bool remove = false;
                const bool cont = callback(this->storage[i], &remove, ctx);

                // remove the object at this index
                if(remove) {
                    this->remove(i);
                }
                // otherwise, advance to next index
                else {
                    i++;
                }

                // bail it that was the last invocation
                if(!cont) return;
            }
        }

    private:
        /**
         * Returns the index of the parent of the node at `i`
         *
         * @note The result is undefined behavior if `i` is 0 for the root node
         */
        constexpr static inline size_t Parent(const size_t i) {
            return (i - 1) / 2;
        }
        /**
         * Returns the index of the left child of the node at index `i`
         */
        constexpr static inline size_t Left(const size_t i) {
            return (2 * i) + 1;
        }
        /**
         * Returns the index of the right child of the node at index `i`
         */
        constexpr static inline size_t Right(const size_t i) {
            return (2 * i) + 2;
        }

        /**
         * Swap the objects at the two indices.
         */
        static inline void swap(T &a, T &b) {
            T tmp(a);
            a = b;
            b = tmp;
        }

        /**
         * Performs the heapify up operation, starting at the given index.
         */
        void heapifyUp(const size_t i) {
            if(i && this->storage[Parent(i)] > this->storage[i]) {
                swap(this->storage[i], this->storage[Parent(i)]);

                // fix up the parent next
                this->heapifyUp(Parent(i));
            }
        }

        /**
         * Performs the heapify down operation, starting at the given index.
         */
        void heapifyDown(const size_t i) {
            const auto left = Left(i);
            const auto right = Right(i);

            auto smallest = i;

            // find the smaller of the left or right child
            if(left < this->size() && this->storage[left] < this->storage[i]) {
                smallest = left;
            } if(right < this->size() && this->storage[right] < this->storage[smallest]) {
                smallest = right;
            }

            // swap with a smaller child and continue down-heapifying
            if(smallest != i) {
                swap(this->storage[i], this->storage[smallest]);
                this->heapifyDown(smallest);
            }
        }

    public:
        /**
         * Remove the element at the given index
         */
        void remove(const size_t i) {
            bool down = true;

            // determine if we need to down or up-heapify later
            if(this->storage[i] > this->storage.back()) {
                down = false; // up heapify if v(i) > v(n-1)
            }

            // swap the elements
            swap(this->storage[i], this->storage.back());
            this->storage.pop_back();

            // fix heapification
            if(down) {
                this->heapifyDown(i);
            } else {
                this->heapifyUp(i);
            }
        }


    private:
    public:
        rt::Vector<T> storage;
};
}

#endif
