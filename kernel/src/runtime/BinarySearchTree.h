#ifndef KERNEL_RUNTIME_BST_H
#define KERNEL_RUNTIME_BST_H

#include <cstdint>
#include <concepts>

#include <log.h>

namespace rt {
/**
 * The content nodes for the tree must satisfy the following constraints:
 *
 * - Implement a `getKey()` method that returns a comparable type with total ordering
 * - Have left/right pointers, as well as a parent pointer
 *
 * Additionally, a copy assignment operator should be implemented.
 */
template<typename T>
concept BSTNode = requires(T a) {
    // method to get the key
    { a.getKey() } -> std::same_as<uintptr_t>;
    // XXX: this is really better but doesn't work yet...
    // { a.getKey() } -> std::totally_ordered<uintptr_t>;

    // pointers required for the structure of the tree
    { a.parent } -> std::same_as<T *>;
    { a.left }   -> std::same_as<T *>;
    { a.right }  -> std::same_as<T *>;
};

/**
 * A generic implementation of a binary search tree, using an arbitrary user specified leaf type.
 *
 * It assumes that the tree nodes can be allocated normally from the standard kernel heap.
 */
template<BSTNode Leaf, typename KeyType = uintptr_t>
class BinarySearchTree {
    public:
        /**
         * Delete all tree nodes when the tree is deallocated.
         */
        ~BinarySearchTree() {
            this->deleteSubtree(this->root);
        }

        /**
         * Determine whether the tree contains any data.
         */
        inline const bool empty() const {
            return !this->root;
        }

        /**
         * Returns the total number of nodes in the tree.
         */
        inline const size_t size() const {
            return this->countNodes(this->root);
        }

        /**
         * Returns the tree node for a particular key.
         */
        inline Leaf *findKey(const KeyType key) {
            return this->findKey(key, this->root);
        }

        /**
         * Inserts a new leaf into the tree, maintaining its order.
         *
         * @param key Key to insert the object under; this replaces any already existing value in
         * the leaf.
         * @param leaf An allocated tree leaf node object, which can be later deleted using the
         * default deleter.
         */
        inline void insert(const KeyType key, const Leaf *leaf) {
            this->insert(key, leaf, this->root);
        }

        /**
         * Removes an object with a particular key.
         *
         * @return Whether the object was found and removed from the tree.
         */
        inline const bool remove(const KeyType key) {
            return this->remove(key, this->root);
        }

    protected:
        /**
         * Recursively searches the subtree rooted at the given node for an object with the given
         * key.
         */
        Leaf *findKey(const KeyType key, Leaf *node) {
            // No more leaves to test in this subtree
            if(!node) return nullptr;
            // Is the passed in node what we're looking for?
            else if(node->getKey() == key) return node;

            // recurse down the correct side of the tree
            if(key < node->getKey()) {
                return this->findKey(key, node->left);
            } else {
                return this->findKey(key, node->right);
            }
        }

        /**
         * Inserts the given node in a particular subtree. The tree is recursed until an
         * appropriate leaf is found, to which the given node is added as a child.
         *
         * @param key Key to sort the object by
         * @param leaf Tree node to insert
         * @param root Current node to process
         * @param parent Leaf immediately above the current level, or nullptr at root
         */
        void insert(const KeyType key, Leaf *leaf, Leaf* &root, Leaf *parent = nullptr) {
            // subtree is null
            if(!root) {
                leaf->parent = parent;
                root = leaf;
            }
            // key is equal (replace node)
            else if(root->getKey() == key) {
                // XXX: what the fuck
                *root = *leaf;
                panic("u wot m8");
            }
            // go down left subtree
            else if(root->getKey() > key) {
                return this->insert(key, leaf, root->left, root);
            }
            // go down right subtree
            else {
                return this->insert(key, leaf, root->right, root);
            }
        }

        /**
         * Searches a particular subtree for an object with the given key, then removes the node
         * containing it.
         *
         * @param key Key of the object to remove
         * @param node Subtree to search
         * @param parent Parent node to the currently processing
         * 
         * @return Whether an object with the key found and removed.
         */
        bool remove(const KeyType key, Leaf *node, Leaf *parent = nullptr) {
            // have we reached the end of the subtree?
            if(!node) return false;

            // recurse down later subtrees if needed
            if(key < node->getKey()) {
                return this->remove(key, node->left, node);
            } else if(key > node->getKey()) {
                return this->remove(key, node->right, node);
            }

            // node has two children
            if(node->left && node->right) {
                auto successor = this->findSuccessor(node);
                // XXX: ensure successor is non-nil; or is this an invariant?

                // copy the successor's key/value
                *node = *successor;

                // then remove the duplicate successor key from the right subtree
                return this->remove(successor->getKey(), node->right, node);
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
         * Returns the minimum node in a given subtree. This simply keeps going down the left
         * subtree of a particular node until we reach the end.
         *
         * @param node Subtree to find the minimum value in
         *
         * @return Tree node corresponding to the minimum value in the given subtree
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
         * Traverses the contents of the tree in order.
         */
        void iterateInOrder(Leaf *node, void (*callback)(const Leaf &)) {
            // bail if empty node
            if(!node) return;

            // go down left subtree
            this->iterateInOrder(node->left, callback);
            // node value itself
            callback(*node); // XXX: should we pass a pointer instead?
            // go down right subtree
            this->iterateInOrder(node->right, callback);
        }

    protected:
        /// Root node of the tree (if it is not empty)
        Leaf *root = nullptr;
};
}

#endif
