#ifndef KERNEL_VM_MAPTREE_H
#define KERNEL_VM_MAPTREE_H

#include "MapEntry.h"

namespace vm {
/**
 * An AVL tree implementation specifically geared towards use by the virtual memory subsystem to
 * store a process' VM map. (TODO: this is currently just a BST, not an AVL tree. fix this lol)
 *
 * Each node in the tree stores not only the per-process virtual base address and a pointer to the
 * VM object it maps, but also the size of a "window" allocated in the address space for that
 * object, as well as any modifier flags.
 */
class MapTree {
    public:
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
        struct Leaf {
            friend class MapTree;

            /// Base address for this mapping
            uintptr_t address = 0;
            /// Size of the mapping, in bytes
            size_t size = 0;
            /// Optional mapping flags
            MappingFlags flags = MappingFlags::None;
            /// VM object backing this mapping
            MapEntry *entry = nullptr;

            // private: // XXX: commented for debugging purposes
                /// Parent node of this leaf (if not root)
                Leaf *parent = nullptr;
                /// Left child node (if any)
                Leaf *left = nullptr;
                /// Right child node (if any)
                Leaf *right = nullptr;

            public:
                Leaf() = default;
                Leaf(Leaf *_parent) : parent(_parent) {}
                Leaf(const uintptr_t _address, const size_t _size, const MappingFlags _flags,
                        MapEntry *_entry, Leaf *_parent = nullptr) : address(_address),
                size(_size), flags(_flags), entry(_entry), parent(_parent) {}

                /// Nodes compare equal if their addresses are equal
                bool operator==(const Leaf &in) const {
                    return this->address == in.address;
                }

                /// Copies a tree node's payload (address and entry ptr)
                Leaf &operator=(const Leaf &in) {
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

    public:
        /**
         * Delete all tree nodes.
         */
        ~MapTree() {
            this->deleteSubtree(this->root);
        }

        /**
         * Returns the total number of nodes in the tree.
         */
        size_t size() const {
            return this->countNodes(this->root);
        }

        /**
         * Searches for a virtual memory mapping with the given base address in the tree.
         *
         * @return VM object at that base address, or `nullptr` if not found.
         */
        MapEntry *findBase(const uintptr_t address) {
            auto leaf = this->findBase(address, this->root);
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
        const uintptr_t baseAddressFor(const MapEntry *entry) const {
            auto leaf = this->leafFor(entry, this->root);
            return leaf ? leaf->address : 0;
        }
        /**
         * Locates the base address and mapping window length of a particular VM object.
         */
        const uintptr_t baseAddressFor(const MapEntry *entry, size_t &outLength) const {
            auto leaf = this->leafFor(entry, this->root);
            outLength = leaf ? leaf->size : 0;
            return leaf ? leaf->address : 0;
        }
        /**
         * Locates the base address, mapping window length and mapping flags of a particular VM
         * object.
         */
        const uintptr_t baseAddressFor(const MapEntry *entry, size_t &outLength,
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
        MapEntry *find(const uintptr_t address, Leaf **outLeaf = nullptr) {
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
        MapEntry *find(const uintptr_t address, size_t &offset) {
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
        void insert(const uintptr_t address, const size_t size, MapEntry *entry, MappingFlags flags = MappingFlags::None) {
            this->insert(this->root, address, size, entry, flags, this->root);
        }

        /**
         * Remove a virtual memory mapping with a particular base address.
         *
         * @param address Base address of the mapping to remove.
         * 
         * @return Whether a mapping with the given address was found and removed.
         */
        bool remove(const uintptr_t address) {
            return this->remove(this->root, nullptr, address);
        }

        /**
         * Performs an in-order traversal of the tree, invoking the given callback for each of the
         * nodes in the tree.
         *
         * @param callback Function to invoke with the base address and map pointer of each entry.
         */
        void iterate(void (*callback)(const Leaf &)) {
            this->iterate(this->root, callback);
        }

    private:
        /**
         * Recursively deletes the left and right subtrees of the given tree node, then deletes
         * the node itself.
         */
        void deleteSubtree(Leaf *node) {
            if(!node) return;

            this->deleteSubtree(node->left);
            this->deleteSubtree(node->right);
            delete node;
        }

        /**
         * Recursively counts the number of nodes in the tree.
         */
        size_t countNodes(Leaf *node) const {
            // no more nodes
            if(!node) return 0;

            // check its subnodes
            return 1 + this->countNodes(node->left) + this->countNodes(node->right);
        }

        /**
         * Searches all nodes in order until one backed by the given VM object is located. Return
         * its tree node.
         */
        const Leaf *leafFor(const MapEntry *entry, Leaf *leaf) const {
            // test if we've reached the end of subtree or found the entry
            if(!leaf) return nullptr;
            else if(leaf->entry == entry) return leaf;

            // recurse down the left and right subtrees
            auto left = this->leafFor(entry, leaf->left);
            if(left) return left;

            return this->leafFor(entry, leaf->right);
        }


        /**
         * Searches a subtree, rooted at `node`, for a virtual memory mapping with the given base
         * address.
         *
         * @param address Base address to locate a memory mapping for
         * @param node Tree node at which to begin the search
         *
         * @return Tree node corresponding to the mapping or `nullptr` if not found
         */
        Leaf *findBase(const uintptr_t address, Leaf *node) {
            // No more leaves to test in this subtree
            if(!node) return nullptr;
            // Is the passed in node what we're looking for?
            else if(node->address == address) return node;

            // recurse down the correct side of the tree
            if(address < node->address) {
                return this->findBase(address, node->left);
            } else {
                return this->findBase(address, node->right);
            }
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
        Leaf *find(const uintptr_t address, Leaf *node) {
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
        Leaf *findOverlapping(const uintptr_t base, const size_t length, Leaf *node) {
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

        /**
         * Inserts a new node into the subtree rooted at the given node.
         *
         * The node is inserted in the left subtree if less than the node provided, or in the right
         * subtree if the address is greater.
         *
         * @param address Base address for the memory mapping
         * @param entry VM object to map
         */
        void insert(Leaf* &root, const uintptr_t address, const size_t size, MapEntry *entry, const MappingFlags flags, Leaf *parent) {
            // subtree is null
            if(!root) {
                root = new Leaf(address, size, flags, entry, parent);
            }
            // address is equal (replace mapping)
            else if(root->address == address) {
                // XXX: what the fuck
                root->entry = entry;
                panic("u wot m8");
            }
            // go down left subtree
            else if(root->address > address) {
                return this->insert(root->left, address, size, entry, flags, root);
            }
            // go down right subtree
            else {
                return this->insert(root->right, address, size, entry, flags, root);
            }
        }


        /**
         * Searches a particular subtree for a virtual memory mapping with the given base address,
         * then removes the node containing it.
         *
         * @param node Subtree to search
         * @param parent Parent node to the currently processing
         * @param address Base address of the mapping to remove.
         * 
         * @return Whether a mapping with the given address was found and removed.
         */
        bool remove(Leaf *node, Leaf *parent, const uintptr_t address) {
            // have we reached the end of the subtree?
            if(!node) return false;

            // recurse down later subtrees if needed
            if(address < node->address) {
                return this->remove(node->left, node, address);
            } else if(address > node->address) {
                return this->remove(node->right, node, address);
            }

            // node has two children
            if(node->left && node->right) {
                auto successor = this->findSuccessor(node);
                // XXX: ensure successor is non-nil; or is this an invariant?

                // copy the successor's key/value
                *node = *successor;

                // then remove the duplicate successor key from the right subtree
                return this->remove(node->right, node, successor->address);
            }
            // node has only a left child: simply replace it
            else if(node->left && !node->right) {
                if(parent->left == node) parent->left = node->left;
                else if(parent->right == node) parent->right = node->left;
                node->left->parent = parent;
            }
            // node has only a right child
            else if(!node->left && node->right) {
                if(parent->left == node) parent->left = node->right;
                else if(parent->right == node) parent->right = node->right;
                node->right->parent = parent;
            }
            // node has no children
            else {
                // remove from parent
                if(parent) {
                    if(parent->left == node) parent->left = nullptr;
                    else if(parent->right == node) parent->right = nullptr;
                }
                // it's the root node
                else {
                    this->root = nullptr;
                }
            }

            // for the single or no child nodes case: clean up the original node
            delete node;
            return true;
        }


        /**
         * Returns the minimum node in a given subtree. Leafhis simply keeps going down the left
         * subtree of a particular node until we reach the end.
         *
         * @param node Subtree to find the minimum value in
         *
         * @return Leafree node corresponding to the minimum value in the given subtree
         */
        Leaf *findMin(Leaf *node) {
            if(!node) return nullptr;

            if(node->left) return this->findMin(node->left);
            else return node;
        }

        /**
         * Returns the maximum node in a given subtree.
         *
         * @param node Subtree to find the minimum value in
         */
        Leaf *findMax(Leaf *node) {
            if(!node) return nullptr;

            if(node->right) return this->findMax(node->right);
            else return node;
        }

        /**
         * Returns the successor, i.e. the next key in the sorted order, to the given node.
         *
         * @param node Node whose successor to find
         *
         * @return Successor to the given node
         */
        Leaf *findSuccessor(Leaf *node) {
            if(node->right) {
                return this->findMin(node->right);
            }

            return nullptr;
        }

        /**
         * Returns the predecessor, i.e. the previous key in sorted order, to the given node.
         */
        Leaf *findPredecessor(Leaf *node) {
            if(node->left) {
                return this->findMax(node->left);
            }

            return nullptr;
        }

        /**
         * Implements in-order traversal of the given subtree
         */
        void iterate(Leaf *node, void (*callback)(const Leaf &)) {
            // bail if empty node
            if(!node) return;

            // go down left subtree
            this->iterate(node->left, callback);
            // node value itself
            callback(*node); // XXX: should we pass a pointer instead?
            // go down right subtree
            this->iterate(node->right, callback);
        }

    private:
        /// Root node of the VM mapping 
        Leaf *root = nullptr;
};
}

#endif
