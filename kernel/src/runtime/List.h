#ifndef KERNEL_RUNTIME_LIST_H
#define KERNEL_RUNTIME_LIST_H

#include <stddef.h>

#include <log.h>
#include <string.h>
#include <new>

namespace rt {
/**
 * Basic doubly-linked list implementation. Allows random access (at O(n) cost) from either end.
 *
 * Insertion at the front or back is a constant time O(1) operation; inserting an element in the
 * middle of the list has a cost of O(n).
 */
template<class T>
class List {
    private:
        /// Linked list element
        struct Element {
            T value;

            /// pointer to previous element, or nullptr if head of list
            Element *prev = nullptr;
            /// pointer to next element, or nullptr if tail of list
            Element *next = nullptr;

            Element() = default;
            Element(const T &_value) : value(_value) {}
        };

        /// Iterator type
        struct Iterator {
            friend class List<T>;

            private:
                /// Sets up the iterator
                Iterator(List *_list, const Element *_item) : list(_list) , element(_item){}

            public:
                /// This does nothing but we must declare it anyways
                ~Iterator() = default;

            private:
                /// List we're referencing data on
                List *list = nullptr;
                /// Element that this iterator points to (nullptr = end)
                Element *element = nullptr;
        };

    public:
        /**
         * Deallocates all list elements.
         */
        ~List() {
            auto head = this->head;
            while(head) {
                auto next = head->next;
                delete head;
                head = next;
            }
        }

        /**
         * Appends an item to the end of the list.
         */
        void append(const T &value) {
            auto el = new Element(value);

            // if we have a tail element, the list can't be empty.
            if(this->tail) {
                this->tail->next = el;
                el->prev = this->tail;
                this->tail = el;
            }
            // otherwise, we have to set it as the head and tail of the list
            else {
                this->tail = el;
                this->head = el;
            }

            this->numElements++;
        }

        /**
         * Removes the item at the given index.
         */
        void remove(const size_t index) {
            REQUIRE(index < this->numElements, "list index out of bounds (%zu)", index);
            // head of list?
            if(index == 0) {
                auto currentHead = this->head;

                this->head = currentHead->next;
                this->head->prev = nullptr;

                delete currentHead;
            }
            // otherwise, iterate the list
            else {
                panic("removing elements inside list not yet supported");
            }

            this->numElements--;
        }

        /**
         * Iterates all items in the list, removing those that match certain criteria.
         */
        // void iterate();

        /// Gets a reference to the given item
        const T& operator[](const size_t index) const {
            REQUIRE(index < this->numElements, "list access out of bounds: %zu", index);
            if(!index) {
                return this->head->value;
            }

            size_t i = 0;
            auto el = this->head;
            while(el) {
                if(i++ == index) {
                    return el->value;
                }
                el = el->next;
            }

            // we should never get here
            panic("list access failed");
        }

        /// Is the list empty?
        const bool empty() const {
            return (this->head == nullptr);
        }
        /// Number of items in the list
        const size_t size() const {
            return this->numElements;
        }
    private:
        /// Head of list, or null if empty
        Element *head = nullptr;
        /// Last element of list, or null if empty
        Element *tail = nullptr;

        /// Total number of items in the list
        size_t numElements = 0;
};
}

#endif
