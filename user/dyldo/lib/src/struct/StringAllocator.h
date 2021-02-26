#ifndef DYLDO_STRUCT_STRINGALLOCATOR_H
#define DYLDO_STRUCT_STRINGALLOCATOR_H

#include "Linker.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <list>

namespace dyldo {
/**
 * String allocator
 *
 * This is basically a slab allocator, which allocates fixed size chunks of memory, in which the
 * actual strings are stored.
 *
 * Strings are stored zero-terminated for ease of use.
 */
class StringAllocator {
    public:
        /**
         * Inserts a new string fragment into the allocator.
         *
         * @return The segment's memory address, or NULL on failure
         */
        const char *_Nullable add(const char * _Nonnull str, const size_t _len = 0) {
            // get input string length
            size_t len = _len;
            if(!len) {
                len = strlen(str);
            }

            // can we find an existing slab to service the allocation?
            for(auto &slab : this->slabs) {
                if(slab.getAvailable() > len) {
                    return slab.append(str, len);
                }
            }

            // if not, allocate a new slab and try again
            this->slabs.emplace_back();
            return this->add(str, len);
        }

    private:
        /// Slab size
        constexpr static const size_t kSlabSz = (1024 * 16);

        struct Slab {
            /// storage array
            char * _Nonnull storage;
            /// number of bytes allocated
            size_t numAllocated = 0;

            Slab() {
                this->storage = reinterpret_cast<char *>(calloc(kSlabSz, 1));
                if(!this->storage) Linker::Abort("out of memory");
            }

            ~Slab() {
                free(this->storage);
            }

            /**
             * Appends a string to the end of the slab's allocation region.
             */
            const char *_Nullable append(const char * _Nonnull str, const size_t len) {
                // remember null terminator!
                if((len + 1) > this->getAvailable()) {
                    return nullptr;
                }
                // copy it
                auto start = this->storage + this->numAllocated;
                memcpy(start, str, len);
                start[len] = '\0';

                this->numAllocated += (len + 1);

                return start;
            }

            /// number of free bytes
            const size_t getAvailable() const {
                return (kSlabSz - this->numAllocated);
            }
        };

    private:
        /// All allocated slabs
        std::list<Slab> slabs;
};
}

#endif
