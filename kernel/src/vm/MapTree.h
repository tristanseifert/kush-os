#ifndef KERNEL_VM_MAPTREE_H
#define KERNEL_VM_MAPTREE_H

#include "runtime/BinarySearchTree.h"
#include "runtime/SmartPointers.h"

#include "MapEntry.h"

namespace vm {
/**
 * Encapsulates the information for a single virtual memory mapping in the VM tree.
 *
 * Note that the size value is NOT automatically updated: this allows VM objects to be
 * resized by their owner without concern for the behavior in other tasks. If the object
 * shrinks, accessing pages beyond its new end will fault; if it grows, the new pages are
 * simply inaccessible.
 *
 * @note All addresses and sizes must be page aligned.
 */
struct MapTreeLeaf {
    friend class MapTree;

    /// Base address for this mapping
    uintptr_t address = 0;
    /// Size of the mapping, in bytes
    size_t size = 0;
    /// Optional mapping flags
    MappingFlags flags = MappingFlags::None;
    /// VM object backing this mapping
    rt::SharedPtr<MapEntry> entry;

    // private: // XXX: commented for debugging purposes
        /// Parent node of this leaf (if not root)
        MapTreeLeaf *parent = nullptr;
        /// Left child node (if any)
        MapTreeLeaf *left = nullptr;
        /// Right child node (if any)
        MapTreeLeaf *right = nullptr;

    public:
        MapTreeLeaf() = default;
        MapTreeLeaf(MapTreeLeaf *_parent) : parent(_parent) {}
        MapTreeLeaf(const uintptr_t _address, const size_t _size, const MappingFlags _flags,
                rt::SharedPtr<MapEntry> _entry, MapTreeLeaf *_parent = nullptr) : 
            address(_address), size(_size), flags(_flags), entry(_entry), parent(_parent) {}

        /// Tree sort key
        const uintptr_t getKey() const {
            return this->address;
        }

        /// Nodes compare equal if their addresses are equal
        bool operator==(const MapTreeLeaf &in) const {
            return this->address == in.address;
        }

        /// Copies a tree node's payload (address and entry ptr)
        MapTreeLeaf &operator=(const MapTreeLeaf &in) {
            this->address = in.address;
            this->size = in.size;
            this->flags = in.flags;
            this->entry = in.entry;

            return *this;
        }

        /// Check whether the given address is is contained in this map.
        bool contains(const uintptr_t address) const {
            return (address >= this->address) && ((address - this->address) < this->size);
        }

        /// Check whether the given address range overlaps this mapping.
        bool contains(const uintptr_t address, const size_t length) const {
            const auto x1 = this->address;
            const auto x2 = (this->address + this->size - 1);
            const auto y1 = address;
            const auto y2 = (address + length);

            return (x1 >= y1 && x1 <= y2) ||
                   (x2 >= y1 && x2 <= y2) ||
                   (y1 >= x1 && y1 <= x2) ||
                   (y2 >= x1 && y2 <= x2);
        }
};



/**
 * An AVL tree implementation specifically geared towards use by the virtual memory subsystem to
 * store a process' VM map. (TODO: this is currently just a BST, not an AVL tree. fix this lol)
 *
 * Each node in the tree stores not only the per-process virtual base address and a pointer to the
 * VM object it maps, but also the size of a "window" allocated in the address space for that
 * object, as well as any modifier flags.
 */
class MapTree: public rt::BinarySearchTree<MapTreeLeaf> {
    public:
        /**
         * Searches for a virtual memory mapping with the given base address in the tree.
         *
         * @return VM object at that base address, or `nullptr` if not found.
         */
        rt::SharedPtr<MapEntry> findBase(const uintptr_t address) {
            auto leaf = this->findKey(address, this->root);
            return leaf ? leaf->entry : nullptr;
        }

        /**
         * Locates the base address of a particular VM object.
         *
         * This is implemented as an in-order traversal of the entire tree. So it's not a
         * particularly fast operation.
         *
         * @return Base address of the VM object, or 0 if not found. (Maps at addr 0 are dumb)
         */
        const uintptr_t baseAddressFor(const rt::SharedPtr<MapEntry> entry) const {
            auto leaf = this->leafFor(entry, this->root);
            return leaf ? leaf->address : 0;
        }
        /**
         * Locates the base address and mapping window length of a particular VM object.
         */
        const uintptr_t baseAddressFor(const rt::SharedPtr<MapEntry> entry, size_t &outLength) const {
            auto leaf = this->leafFor(entry, this->root);
            outLength = leaf ? leaf->size : 0;
            return leaf ? leaf->address : 0;
        }
        /**
         * Locates the base address, mapping window length and mapping flags of a particular VM
         * object.
         */
        const uintptr_t baseAddressFor(const rt::SharedPtr<MapEntry> entry, size_t &outLength,
                MappingFlags &outFlags) const {
            auto leaf = this->leafFor(entry, this->root);
            outLength = leaf ? leaf->size : 0;
            outFlags = leaf ? leaf->flags : MappingFlags::None;
            return leaf ? leaf->address : 0;
        }

        /**
         * Locates the VM mapping object that contains the given virtual address.
         *
         * @param address Virtual address to locate a mapping for
         * @param outLeaf If specified, a pointer to the tree node corresponding to the located
         *                VM object is stored here.
         *
         * @return VM object containing the given address, or `nullptr` if not found.
         */
        rt::SharedPtr<MapEntry> find(const uintptr_t address, MapTreeLeaf **outLeaf = nullptr) {
            auto leaf = this->find(address, this->root);
            if(outLeaf) *outLeaf = leaf;
            return leaf ? leaf->entry : nullptr;
        }

        /**
         * Locates the VM mapping object that contains the given address and calculates the offset
         * into the object.
         *
         * @param address Virtual address to locate a mapping for
         * @param offset Byte offset into the VM object (i.e. address - base)
         *
         * @return VM object containing the given address, or `nullptr` if not found.
         */
        rt::SharedPtr<MapEntry> find(const uintptr_t address, size_t &offset) {
            auto leaf = this->find(address, this->root);
            if(leaf) offset = (address - leaf->address);
            return leaf ? leaf->entry : nullptr;
        }

        /**
         * Determines if the given region of [start, start+length) conflicts with any existing
         * mappings in the tree.
         *
         * @param start Base virtual address of the region to test
         * @param length Number of bytes in the virtual address region to check
         * @param outNext Address of the end of the conflicting region
         *
         * @return Whether there were any mappings found that overlap the given range.
         */
        bool isRegionFree(const uintptr_t base, const size_t length, uintptr_t &outNext) {
            auto leaf = this->findOverlapping(base, length, this->root);

            outNext = leaf ? (leaf->address + leaf->size) : 0;
            return leaf == nullptr;
        }

        /**
         * Inserts into the tree a new virtual memory object.
         *
         * @param address Virtual address for the base of the mapping
         * @param size Size of the virtual memory mapping, in bytes
         * @param entry VM object to map at this location
         * @param flags Additional flags to define the mapping; if specified, they replace the
         * permissions of the VM object.
         */
        void insert(const uintptr_t address, const size_t size,
                const rt::SharedPtr<MapEntry> &entry, MappingFlags flags = MappingFlags::None) {
            auto node = new MapTreeLeaf(address, size, flags, entry);
            this->insert(address, node, this->root);
        }

        /**
         * Remove a virtual memory mapping with a particular base address.
         *
         * @param address Base address of the mapping to remove.
         * 
         * @return Whether a mapping with the given address was found and removed.
         */
        inline bool remove(const uintptr_t address) {
            return this->remove(address, this->root);
        }

        /**
         * Performs an in-order traversal of the tree, invoking the given callback for each of the
         * nodes in the tree.
         *
         * @param callback Function to invoke with the base address and map pointer of each entry.
         */
        void iterate(void (*callback)(const MapTreeLeaf &)) {
            this->iterateInOrder(this->root, callback);
        }

    private:
        // allow finding some super implementations
        using BinarySearchTree::remove;
        using BinarySearchTree::insert;

        /**
         * Searches all nodes in order until one backed by the given VM object is located. Return
         * its tree node.
         */
        const MapTreeLeaf *leafFor(const rt::SharedPtr<MapEntry> entry, MapTreeLeaf *leaf) const {
            // test if we've reached the end of subtree or found the entry
            if(!leaf) return nullptr;
            else if(leaf->entry == entry) return leaf;

            // recurse down the left and right subtrees
            auto left = this->leafFor(entry, leaf->left);
            if(left) return left;

            return this->leafFor(entry, leaf->right);
        }

        /**
         * Searches the subtree, rooted at `node`, for a virtual memory mapping that contains the
         * specified address.
         *
         * @param address Virtual address to find the responsible mapping for
         * @param outLeaf If specified, a varaible to store the tree node containing the entry
         * @param node Leaf at which we should begin the search
         *
         * @return Tree leaf corresponding to the mapping or `nullptr` if not found
         */
        MapTreeLeaf *find(const uintptr_t address, MapTreeLeaf *node) {
            if(!node) return nullptr;
            else if(node->address == address) return node;

            // address is smaller so there's no chance it can be in this node
            if(address < node->address) {
                return this->find(address, node->left);
            }
            // address is greater than or equal; check if it's contained within
            else {
                if(node->contains(address)) {
                    return node;
                }

                // beyond the end of the node, so check the right subtree
                return this->find(address, node->right);
            }
        }

        /**
         * Finds the first mapping that overlaps the given range, if any.
         * TODO: optimize to avoid visiting every node in the tree...
         *
         * @return Node corresponding to the first overlapping map, or `nullptr`
         */
        MapTreeLeaf *findOverlapping(const uintptr_t base, const size_t length, MapTreeLeaf *node) {
            // bail if end of subtree
            if(!node) return nullptr;

            // does this node overlap the range?
            else if(node->contains(base, length)) {
                return node;
            }

            // recurse down the remainder of the tree
            auto left = this->findOverlapping(base, length, node->left);
            if(left) return left;

            return this->findOverlapping(base, length, node->right);
        }
};
}

#endif
