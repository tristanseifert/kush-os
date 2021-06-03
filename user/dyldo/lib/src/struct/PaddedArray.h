#ifndef DYLDO_STRUCT_PADDEDARRAY_H
#define DYLDO_STRUCT_PADDEDARRAY_H

#include "Linker.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace dyldo {
/**
 * Some ELF structures may be larger than the minimum size of the struct, and we want to maintain
 * array semantics even with inter-element padding. This implements some operator magic to provide
 * this behavior.
 *
 * Conceptually, this is very similar to std::span. It does not take ownership of the pointed-to
 * data.
 */
template<class T>
struct PaddedArray {
    public:
        struct Iterator {
            using iterator_category     = std::forward_iterator_tag;
            using difference_type       = std::ptrdiff_t;
            using value_type            = T;
            using pointer               = value_type*;
            using reference             = value_type&;

            Iterator(pointer _ptr, const size_t _stride) : ptr(_ptr), stride(_stride) {}

            inline reference operator*() const { return *this->ptr; }
            inline pointer operator->() { return this->ptr; }

            Iterator& operator++() {
                this->ptrptr = reinterpret_cast<pointer>(reinterpret_cast<uintptr_t>(this->ptr)
                        + this->stride);
                return *this;
            }
            Iterator operator++(int) {
                Iterator temp = *this;
                ++(*this);
                return temp;
            }

            friend bool operator== (const Iterator& a, const Iterator& b) { return a.ptr == b.ptr; };
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.ptr != b.ptr; }; 

            private:
                pointer ptr = nullptr;
                size_t stride = 0;
        };

        struct ConstIterator {
            using iterator_category     = std::forward_iterator_tag;
            using difference_type       = std::ptrdiff_t;
            using value_type            = T;
            using pointer               = value_type*;
            using reference             = value_type&;

            ConstIterator(pointer _ptr, const size_t _stride) : ptr(_ptr), stride(_stride) {}

            inline reference operator*() const { return *this->ptr; }
            inline const pointer operator->() const { return this->ptr; }

            ConstIterator& operator++() {
                this->ptr = reinterpret_cast<pointer>(reinterpret_cast<uintptr_t>(this->ptr)
                        + this->stride);
                return *this;
            }
            ConstIterator operator++(int) {
                ConstIterator temp = *this;
                ++(*this);
                return temp;
            }

            friend bool operator== (const ConstIterator& a, const ConstIterator& b) { return a.ptr == b.ptr; };
            friend bool operator!= (const ConstIterator& a, const ConstIterator& b) { return a.ptr != b.ptr; }; 

            private:
                pointer ptr = nullptr;
                size_t stride = 0;
        };
    public:
        /**
         * Create a padded array with no elements that has a null pointer.
         */
        PaddedArray() = default;

        /**
         * Create a padded array with the given memory and element stride.
         */
        PaddedArray(T *_data, const size_t _size, const size_t _stride = sizeof(T)) : data(_data),
            elements(_size), stride(_stride) {
            if(_stride < sizeof(T)) {
                Linker::Abort("Invalid PaddedArray stride %lu (min %lu)", _stride, sizeof(T));
            }
        }

        /**
         * Assigning to a padded array simply does a shallow copy of the inside pointers
         */
        PaddedArray& operator=(const PaddedArray& o) {
            if(this != &o) {
                this->data = o.data;
                this->stride = o.stride;
                this->elements = o.elements;
            }
            return *this;
        }

        /**
         * Get a reference to the ith element
         */
        constexpr T& operator[](const size_t i) {
            if(i >= this->elements) {
                Linker::Abort("PaddedArray out of range %lu", i);
            }

            auto address = reinterpret_cast<uintptr_t>(this->data);
            address += (i * this->stride);
            return *reinterpret_cast<T *>(address);
        }

        /// Return the number of elements in the array
        constexpr inline size_t size() const {
            return this->elements;
        }
        /// Return the total number of bytes of the array
        constexpr inline size_t size_bytes() const {
            return this->size() * this->stride;
        }
        /// Check if the array is empty
        constexpr inline bool empty() const {
            return !this->size();
        }
        /// Returns the stride between elements
        constexpr inline size_t elementStride() const {
            return this->stride;
        }

        /// Return iterator to the first item in the sequence
        Iterator begin() {
            return Iterator(this->data, this->stride);
        }
        /// Return iterator to one past the end of the sequence
        Iterator end() {
            auto address = reinterpret_cast<uintptr_t>(this->data);
            address += (this->elements * this->stride);
            return Iterator(reinterpret_cast<T *>(address), this->stride);
        }

        /// Return const iterator to the first item in the sequence
        ConstIterator begin() const {
            return ConstIterator(this->data, this->stride);
        }
        /// Return iterator to one past the end of the sequence
        ConstIterator end() const {
            auto address = reinterpret_cast<uintptr_t>(this->data);
            address += (this->elements * this->stride);
            return ConstIterator(reinterpret_cast<T *>(address), this->stride);
        }
    private:
        /// Pointer to the element data
        T *data = nullptr;
        /// Number of elements
        size_t elements = 0;
        /// Stride between elements
        size_t stride = 0;
};
}

#endif
