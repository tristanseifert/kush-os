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
                Iterator(const List *_list, const Element *_item) : list(const_cast<List *>(_list)), 
                    element(const_cast<Element *>(_item)) {}
                Iterator(List *_list, Element *_item) : list(_list) , element(_item){}

            public:
                /// This does nothing but we must declare it anyways
                ~Iterator() = default;

                /// Advance the iterator to the next position.
                Iterator &operator++() {
                    this->element = element->next;
                    return *this;
                }
                /// Returns the object we point to.
                T &operator*() {
                    return this->element->value;
                }
                /// Returns the object we point to.
                const T &operator*() const {
                    return this->element->value;
                }
                /// Is this iterator at the same position as another?
                bool operator !=(const Iterator &it) {
                    return (it.element != this->element);
                }

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
         * Inserts an item at the head of the list.
         */
        void prepend(const T &value) {
            auto el = new Element(value);

            // if we have a head element, replace it.
            if(this->head) {
                this->head->prev = el;
                el->next = this->head;
                this->head = el;
            }
            // otherwise, the list is empty
            else {
                this->tail = el;
                this->head = el;
            }

            this->numElements++;
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

                if(currentHead->next) {
                    currentHead->next->prev = nullptr;
                    this->head = currentHead->next;

                    this->numElements--;
                } else {
                    this->head = nullptr;
                    this->tail = nullptr;

                    this->numElements = 0;
                }

                delete currentHead;
            }
            // otherwise, iterate the list
            else {
                panic("removing elements inside list not yet supported");
            }
        }

        /**
         * Removes all objects from the list.
         */
        void clear() {
            auto ptr = this->head;

            while(ptr) {
                auto old = ptr;

                // advance to next element
                ptr = ptr->next;

                // delete this element
                delete old;
            }
        }

        /**
         * Iterates all items in the list, removing those that match certain criteria.
         *
         * The parameters to the callback function are a specified context value, the value in the
         * list item; its return value indicates whether the item is to be removed or not.
         *
         * @return Number of removed items
         */
        template<class arg>
        size_t _removeMatching(bool (*callback)(void *, arg), void *ctx) {
            size_t numRemoved = 0;

            auto ent = this->head;
            while(ent) {
                REQUIRE(ent->next != ent, "invalid next ptr");

                bool remove = callback(ctx, ent->value);
                if(remove) {
                    // update the previous item's next pointer or update head
                    if(ent->prev) {
                        ent->prev->next = ent->next;
                    } else {
                        this->head = ent->next;
                    }
                    // update the next item's previous pointer, or update tail
                    if(ent->next) {
                        ent->next->prev = ent->prev;
                    } else {
                        this->tail = ent->prev;
                    }

                    // delete the old item
                    auto old = ent;
                    ent = ent->next;
                    delete old;

                    numRemoved++;
                    this->numElements--;
                } else {
                    ent = ent->next;
                }
            }

            return numRemoved;
        }

        size_t removeMatching(bool (*callback)(void *, T), void *ctx) {
            return _removeMatching(callback, ctx);
        }
        size_t removeMatching(bool (*callback)(void *, T &), void *ctx) {
            return _removeMatching(callback, ctx);
        }

        /// Gets a reference to the given item
        const T& operator[](const size_t index) const {
            REQUIRE(index < this->numElements, "list access out of bounds: %zu (%zu %p %p)",
                    index, this->numElements, this->head, this->tail);
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

        /// Gets an iterator to the start of the list
        Iterator begin() const {
            return Iterator(this, this->head);
        }
        /// Gets an iterator to the end of the list
        Iterator end() const {
            return Iterator(this, nullptr);
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
