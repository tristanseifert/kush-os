#ifndef KERNEL_RUNTIME_SORTEDLIST_H
#define KERNEL_RUNTIME_SORTEDLIST_H

#include <stddef.h>

#include <new>

#include <log.h>
#include <string.h>
#include <mem/AnonPool.h>

// log allocations
#define LOG_ALLOC                       0
// log insertions
#define LOG_INSERT                      0
// log removals
#define LOG_REMOVE                      1

namespace rt {
/**
 * Defines a list of objects that always sorts its contents in ascending order.
 *
 * For small lists (of `N` items or fewer) there is no additional allocation overhead. If more
 * space is needed, the list will grow by a whole page (consuming it from the kernel anon VM pool)
 * and use that page to store additional objects.
 */
template<class T, size_t N = 32>
class SortedList {
    public:
        /**
         * Initializes the list state.
         */
        SortedList() {

        }

        /**
         * Releases all auxiliary memory allocated by the list.
         */
        ~SortedList() {
            this->deallocOverflow();
        }

        /**
         * Gets the total number of objects contained in the list.
         */
        const size_t size() const {
            size_t size = 0;

            // sum up all overflow pages
            auto next = this->over;
            while(next) {
                size += next->numAllocated;
                next = next->next;
            }

            // and add the number of built-in items
            return size + this->numAllocated;
        }

        /**
         * Checks whether the list is empty.
         */
        const bool empty() const {
            return (this->numAllocated == 0);
        }

        /**
         * Removes all items from the list.
         */
        void reset() {
            this->deallocOverflow();

            // run destructors for any objects in the built in list
            for(size_t i = 0; i < this->numAllocated; i++) {
                this->storage[i].~T();
            }
            this->numAllocated = 0;
        }



        /**
         * Gets a reference to the object at the given index.
         *
         * Complexity: O(N/kNumItems)
         */
        const T& operator[](const size_t index) {
            // is it in the storage region?
            if(index < N) {
                REQUIRE(index < this->numAllocated, "list index (%lu) out of bounds", index);
                return this->storage[index];
            }
            // otherwise, iterate the overflow lists
            size_t offset = index - N;
            auto next = this->over;
            while(next) {
                // if index is in this list, index into it
                if(offset < Overflow::kNumItems) {
                    REQUIRE(offset < next->numAllocated, "overflow page index (%lu) out of bounds", offset);
                    return next->storage[offset];
                }
                // otherwise, check the next overflow page
                else {
                    offset -= Overflow::kNumItems;
                    next = next->next;
                }
            }

            // if we get here, the index is out of bounds
            panic("list index (%lu) out of bounds", index);
        }

        /**
         * Inserts an item into the list at the appropriate location.
         */
        void insert(const T value) {
            size_t insertAt = 0;

            // special case; no items at all
            if(!this->numAllocated) {
                this->storage[0] = value;
                this->numAllocated++;
            }
            // insert into our built-in list if we've space
            else if(this->numAllocated < N) {
                // figure out where to insert it at
                for(size_t i = 0; i < this->numAllocated; i++) {
                    // is the preceding item less than it?
                    if(!i || this->storage[(i-1)] < value) {
                        // is this more than this?
                        if(this->storage[i] > value) {
                            // yup! move the items after it back
                            insertAt = i;
                            const auto toMove = (this->numAllocated - i);

                            memmove(&this->storage[i+1], &this->storage[i], toMove * sizeof(T));

                            // and then insert in the gap we've created
                            goto beach;
                        }
                    }
                }

                // if we fall here, the item should be inserted at the end
                insertAt = this->numAllocated;

beach:;
                // insert the item
                this->storage[insertAt] = value;
                this->numAllocated++;
            }
            // we've got to stick it into overflow space
            else {
                Overflow *page;
                size_t off;

                // allocate our first overflow page if needed
                if(!this->over) {
                    this->allocOverflow();
                }
                // allocate another page if the last page is full
                if(this->last->numAllocated == Overflow::kNumItems) {
                    this->allocOverflow();
                }

                // does the item fit in the sorted range of the built in storage?
                for(size_t i = 0; i < this->numAllocated; i++) {
                    // is the preceding item less than it?
                    if(!i || this->storage[(i-1)] < value) {
                        // is this more than this?
                        if(this->storage[i] > value) {
                            // yup; we can insert here
                            insertAt = i;
                            goto insertion;
                        }
                    }
                }
                // TODO: it doesn't, so iterate over the rest of list
                page = this->over;
                while(page) {
                    T prevValue;

                    // read all entries in this page
                    for(size_t i = 0; i < page->numAllocated; i++) {
                        // read previous value
                        if(i == 0 && page == this->over) {
                            prevValue = this->storage[N-1];
                        } else if(i == 0) {
                            REQUIRE(page->prev, "invalid page %p", page);
                            prevValue = page->prev->storage[Overflow::kNumItems-1];
                        } else {
                            prevValue = page->storage[i-1];
                        }

                        // is the preceding item less than it?
                        if(prevValue < value) {
                            // is the value at the current position less than the new value?
                            if(page->storage[i] > value) {
                                // yeet, insert it here
                                insertAt = page->listIndex + i;
                                goto insertion;
                            }
                        }
                    }

                    // go to the next page
                    page = page->next;
                }

                // if we fall here, it should go at the very end
                insertAt = N;
                page = this->over;
                while(page) {
                    insertAt += page->numAllocated;
                    page = page->next;
                }

insertion:;

                // shift the items after the index by one
                page = this->last;

                while(page) {
                    // is the index inside this page?
                    if(insertAt >= page->listIndex &&
                       insertAt < (page->listIndex + Overflow::kNumItems)) {
                        off = (insertAt - page->listIndex);

                        if(!page->empty()) {
                            // move all items AFTER this one
                            const auto toMove = (page->numAllocated - off);

                            if(page->next) {
                                if(page->next->numAllocated < Overflow::kNumItems) {
                                    page->next->numAllocated++;
                                }
                                page->next->storage[0] = page->storage[Overflow::kNumItems-1];
                            }
                            if(toMove && off+1 < Overflow::kNumItems) {
#if LOG_INSERT
                                log("moving %lu to %lu (count %lu) cap %lu", off, off+1, toMove, page->numAllocated);
#endif
                                if(toMove && toMove < Overflow::kNumItems) {
                                    memmove(&page->storage[off + 1], &page->storage[off],
                                            toMove * sizeof(T));
                                }
                            }
                        }

                        // write the item
                        page->storage[off] = value;

                        if(page->numAllocated < Overflow::kNumItems) {
                            page->numAllocated++;
                        }
#if LOG_INSERT
                        log("yeeting into page %p (off %lu) value %lu capacity %lu", page, off, value, page->numAllocated);
#endif
                    }
                    // if not, shift the entire page down one
                    else {
                        // move the last item in this storage to the first of the previous one
                        if(page->next) {
                            if(page->next->numAllocated < Overflow::kNumItems) {
                                page->next->numAllocated++;
                            }
                            page->next->storage[0] = page->storage[Overflow::kNumItems-1];
                        }

                        // then shift all items down
                        memmove(&page->storage[1], &page->storage[0], 
                                (Overflow::kNumItems - 1) * sizeof(T));
                    }

                    // iterate to the previous one
                    page = page->prev;
                }

                // shift built-in storage if needed
                if(insertAt < N) {
                    const auto toMove = (this->numAllocated - insertAt) - 1;

                    if(this->numAllocated == N) {
                        this->over->storage[0] = this->storage[N-1];

                        if(this->over->numAllocated < Overflow::kNumItems) {
                            this->over->numAllocated++;
                        }
                    }

                    if(toMove) {
#if LOG_INSERT
                        log("yeeting %lu count %lu", insertAt, toMove);
#endif
                        memmove(&this->storage[insertAt+1], &this->storage[insertAt],
                                toMove * sizeof(T));
                    }
                }

                // insert in built-in storage if needed
                if(insertAt < N) {
                    if(this->numAllocated != N) {
                        this->numAllocated++;
                    }

                    this->storage[insertAt] = value;
                }
            }
        }

        /**
         * Removes an item at the given index.
         */
        void remove(const size_t index) {
            bool removed = false;

            // is index in the built in storage?
            if(index < N) {
                REQUIRE(index < this->numAllocated, "list index (%lu) out of bounds", index);

                // move up the items
                const auto toMove = (this->numAllocated - index) - 1;
                if(toMove) {
#if LOG_REMOVE
                    log("shifting built-in storage (remove %lu, move %lu items)", index, toMove);
#endif
                    this->storage[index].~T(); // delete old item
                    memmove(&this->storage[index], &this->storage[index+1], toMove * sizeof(T));
                }

                // decrement the alloc counter if we do not have an overflow page
                if(!this->over) {
                    this->numAllocated--;
                }
                // we do have overflow storage so shift its items too
                else {
                    this->storage[N-1] = this->over->storage[0];

                    auto page = this->over;
                    while(page) {
                        // move up all items
                        if(!page->empty()) {
                            memmove(&page->storage[0], &page->storage[1], (Overflow::kNumItems-1) * sizeof(T));
                        }
                        // copy the first item from the next page
                        if(page->next) {
                            page->storage[Overflow::kNumItems-1] = page->next->storage[0];
                        } 
                        // if no next page, this is the last page and decrement its count
                        else {
                            page->numAllocated --;
                        }

                        // go to next item
                        page = page->next;
                    }
                }
            }
            // it is not in built in storage, so we must iterate pages
            else {
                auto page = this->over;
                while(page) {
                    // is the item in this page?
                    if(index >= page->listIndex &&
                       index < (page->listIndex + Overflow::kNumItems)) {
                        size_t off = (index - page->listIndex);

                        // if so, move up the items after it
                        const auto toMove = (page->numAllocated - off);
                        page->storage[off].~T(); // delete old item
                        memmove(&page->storage[off], &page->storage[off+1], toMove * sizeof(T));

                        removed = true;
                    }
                    // the item was in a previous page
                    else if(index > page->listIndex) {
                        // move up all items
                        if(!page->empty()) {
                            memmove(&page->storage[0], &page->storage[1], (Overflow::kNumItems-1) * sizeof(T));
                        }
                        // copy the first item from the next page
                        if(page->next) {
                            page->storage[Overflow::kNumItems-1] = page->next->storage[0];
                        } 
                    }

                    // decrement count
                    if(!page->next) {
                        page->numAllocated--;
                    }

                    // go to next item
                    page = page->next;
                }

                // ensure the item was removed; if not, it was out of bounds
                REQUIRE(removed, "list index (%lu) out of bounds", index);
            }

            // deallocate the tail overflow page if it's empty
            if(this->last) {
                auto last = this->last;

                if(!last->numAllocated) {
                    last->prev->next = nullptr;
                    this->last = last->prev;

                    mem::AnonPool::freePage(last);
                }
            }
        }

    private:
        /**
         * Allocates a new overflow page and inserts it at the end of the list.
         */
        void allocOverflow() {
            // allocate a page and allocate inside it the overflow struct
            void *page = mem::AnonPool::allocPage();
            REQUIRE(page, "failed to allocate overflow page");

            auto over = new(page) Overflow();

            // insert it at the end
            auto next = this->over;

            if(next) {
                while(next) {
                    // if there's no next page, add it here
                    if(!next->next) {
                        over->prev = next;
                        over->listIndex = next->listIndex + Overflow::kNumItems;

                        next->next = over;
                        goto done;
                    } 
                    // otherwise, move to the next overflow block
                    else {
                        next = next->next;
                    }
                }
            }
            // we don't have an overflow page yet so simply set it as one
            else {
                over->listIndex = N;
                this->over = over;
            }

done:;
            // update the end pointer
            this->last = over;
#if LOG_ALLOC
            log("inserted page %p (prev %p): index %lu", over, over->prev, over->listIndex);

            // iterate them
            auto page2 = this->over;
            while(page2) {
                log("%p prev %p next %p", page2, page2->prev, page2->next);
                page2 = page2->next;
            }
#endif
        }

        /**
         * Deletes all allocated overflow pages.
         */
        void deallocOverflow() {
            auto next = this->over;
            while(next) {
                // get the current pointer and update next
                auto current = next;
                next = next->next;

                // release its memory
                current->~Overflow();
                mem::AnonPool::freePage(current);
            }

            this->over = nullptr;
        }

    private:
        /**
         * Defines the type of an overflow page. This contains as many objects as we can fit into
         * the page, and some metadata to link to the previous/next page.
         *
         * Since we'll always be sorted, there can be no gaps in data, so we can get by with a
         * counter of free slots.
         */
        struct Overflow {
            /// number of items in the storage array
            //constexpr static const size_t kNumItems = ((4096 - 16) / sizeof(T));
            constexpr static const size_t kNumItems = 2;

            /// pointer to next overflow page, or `nullptr` if end of chain
            Overflow *next = nullptr;
            /// pointer to the preceding overflow page, or `nullptr` if first in chain
            Overflow *prev = nullptr;

            /// global list index of the first storage item
            size_t listIndex = 0;
            /// number of allocated objects
            size_t numAllocated = 0;

            /// data storage
            T storage[kNumItems];

            /// whether the overflow page is empty (e.g. nothing is allocated)
            const bool empty() const {
                return (this->numAllocated == 0);
            }
        };

        // TODO: we should use the actual platform page size
        static_assert(sizeof(Overflow) <= 4096, "overflow page too large");
        // XXX: this is kinda hacky but we can't use offsetof() for the true offset inside
        static_assert(offsetof(Overflow, storage) <= 16, "overflow metadata too big");

    private:
        /// number of objects in the storage that have been allocated
        size_t numAllocated = 0;
        /// storage for the internal elements
        T storage[N];

        /// pointer to the first overflow page, if any
        Overflow *over = nullptr;
        /// pointer to the last overflow page (to allow reverse iteration)
        Overflow *last = nullptr;
};
};

#endif
