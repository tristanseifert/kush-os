#ifndef KERNEL_RUNTIME_HASHTABLE_H
#define KERNEL_RUNTIME_HASHTABLE_H

#include "List.h"
#include "Hash.h"

#include <log.h>

namespace rt {
/**
 * Implements a hash table that allows mapping of objects with arbitrary keys. Internally, the
 * table is divided into a fixed number of buckets; each bucket has a list that contains all
 * objects in that bucket.
 *
 * Thus, access is O(1); a fixed amount of time to find the bucket, and, if the hash is well
 * balanced (as it should be) we'll have very few items per bucket making that an almost constant
 * time operation.
 */
template<class KeyType, class ValueType, size_t NumBuckets = 16>
class HashTable {
    private:
        /**
         * Small wrapper around the value that's actually inserted into the buckets; this contains
         * the actual key of the object.
         */
        struct BucketEntry {
            /// key of this object
            KeyType key;
            /// object value
            ValueType value;

            BucketEntry() = default;
            BucketEntry(const KeyType &_key, const ValueType &_value) : key(_key), value(_value) {}
        };

        /// Compares the key against the key in the bucket.
        static bool remover(void *context, BucketEntry &entry) {
            const auto keyPtr = reinterpret_cast<KeyType *>(context);
            return (*keyPtr == entry.key);
        }

        /// info struct for iteration
        struct IterInfo {
            /// callback function
            bool (*callback)(void *, const KeyType &, ValueType &);
            /// context pointer for callback
            void *ctx = nullptr;
        };

    public:
        /**
         * Inserts an item with the given key. If such an object already exists, it is replaced.
         */
        void insert(const KeyType key, const ValueType &value) {
            // determine the bucket
            const auto bucket = this->bucketFor(key);
            auto &list = this->storage[bucket];

            // iterate bucket to remove that item (TODO: optimize)
            if(this->contains(key)) {
                this->remove(key);
            }

            // insert the new item
            BucketEntry ent(key, value);
            list.append(ent);
        }

        /**
         * Removes the object with the given key.
         */
        void remove(const KeyType &key) {
            const auto bucket = this->bucketFor(key);
            auto &list = this->storage[bucket];

            const auto numRemoved = list.removeMatching(&remover, (void *)&key);

            if(!numRemoved) {
                panic("hash table key not found");
            }
        }

        /**
         * Checks whether the table contains a value for the given key.
         */
        const bool contains(const KeyType &key) const {
            const auto bucket = this->bucketFor(key);
            auto &list = this->storage[bucket];

            for(const auto &ent : list) {
                if(ent.key == key) return true;
            }

            return false;
        }

        /**
         * Gets an item with the given key.
         */
        const ValueType &operator[](const KeyType &key) const {
            // determine the bucket
            const auto bucket = this->bucketFor(key);
            auto &list = this->storage[bucket];

            // search it
            for(const auto &ent : list) {
                if(ent.key == key) {
                    return ent.value;
                }
            }

            // not found
            panic("hash table key not found");
        }

        /**
         * Iterates all items in the table.
         *
         * You can return false from the callback to remove that element.
         */
        void iterate(bool (*callback)(void *, const KeyType &, ValueType &), void *ctx) {
            // prepare the info we pass to the iteration
            IterInfo info;
            info.callback = callback;
            info.ctx = ctx;

            // iterate each bucket
            for(size_t i = 0; i < NumBuckets; i++) {
                auto &list = this->storage[i];

                list.removeMatching([](void *ctx, BucketEntry &entry) {
                    const auto info = reinterpret_cast<IterInfo *>(ctx);

                    return info->callback(info->ctx, entry.key, entry.value);
                }, &info);
            }
        }

        /// Gets the total number of items in the table.
        const size_t size() const {
            return this->numItems;
        }

    private:
        /**
         * Gets the bucket for a given key.
         *
         * This invokes the fastboi hash function, interprets the result as a number and simply
         * does a modulus of the number of buckets.
         */
        const size_t bucketFor(const KeyType &key) const {
            const auto hashCode = hash(key);
            return hashCode % NumBuckets;
        }

    private:
        /// Total items in the table
        size_t numItems = 0;

        /// Hash buckets
        List<BucketEntry> storage[NumBuckets];
};
}

#endif
