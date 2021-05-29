#ifndef KERNEL_RUNTIME_REDBLACKTREE_H
#define KERNEL_RUNTIME_REDBLACKTREE_H

#include <cstdint>
#include <concepts>

#include "BinarySearchTree.h"

namespace rt {
/**
 * Color field for a red/black tree node
 */
enum class RBTNodeColor: uint8_t {
    /// node hasn't been assigned a color yet
    None                = 0,
    Red                 = 1,
    Black               = 2,
};
/**
 * Content nodes must satisfy the same constraints as the plain BSTNode, as well as an additional
 * field for the color.
 */
template<typename T>
concept RBTNode = requires(T a) {
    // method to get the key
    { a.getKey() } -> std::same_as<uintptr_t>;

    // pointers required for the structure of the tree
    { a.parent } -> std::same_as<T *>;
    { a.left }   -> std::same_as<T *>;
    { a.right }  -> std::same_as<T *>;

    // node's color
    { a.getColor() } -> std::same_as<RBTNodeColor>;
    //{ a.setColor(RBTNodeColor) } -> void;
};

/**
 * Red/black trees are a flavor of binary search tree that is self-balancing, i.e. any time a leaf
 * is inserted or removed, we'll do some ~ things ~ to make sure the tree is roughly balanced and
 * we get the best case O(log n) lookup time for any given key.
 *
 */
template<RBTNode Leaf, typename KeyType = uintptr_t>
class RedBlackTree: public BinarySearchTree<Leaf, KeyType> {
    using BST = BinarySearchTree<Leaf, KeyType>;

    public:
        using BST::insert;
        using BST::remove;

        void iterate(void (*callback)(const Leaf &)) {
            this->iterateInOrder(this->root, callback);
        }

    private:
        /// Returns the color of the node, or black if the node is a null pointer
        constexpr static auto Color(Leaf *leaf) {
            if(!leaf) return RBTNodeColor::Black;
            return leaf->getColor();
        }

        /**
         * Swaps two leaf nodes.
         */
        void swap(Leaf *u, Leaf *v) {
            if(!u->parent) {
                this->root = v;
            } else if(u == u->parent->left) {
                u->parent->left = v;
            } else {
                u->parent->right = v;
            }

            if(v) v->parent = u->parent;
        }

        /**
         * Rotates the given node leftward.
         */
        void leftRotate(Leaf *x) {
            auto y = x->right;
            x->right = y->left;

            if(y->left) {
                y->left->parent = x;
            }

            y->parent = x->parent;

            if(!x->parent) {
                this->root = y;
            } else if(x == x->parent->left) {
                x->parent->left = y;
            } else {
                x->parent->right = y;
            }

            y->left = x;
            x->parent = y;
        }

        /**
         * Rotates the given node rightward.
         */
        void rightRotate(Leaf *x) {
            auto y = x->left;
            x->left = y->right;

            if(y->right) {
                y->right->parent = x;
            }

            y->parent = x->parent;

            if(!x->parent) {
                this->root = y;
            } else if(x == x->parent->right) {
                x->parent->right = y;
            } else {
                x->parent->left = y;
            }

            y->right = x;
            x->parent = y;
        }

        /**
         * Fixes the red-black tree by recoloring (and possibly rotating) nodes starting with the
         * newly inserted node.
         */
        void repairPostInsert(Leaf *leaf) {
            Leaf *temp;

            while(leaf->parent && Color(leaf->parent) == RBTNodeColor::Red) {
                // right subtree
                if(leaf->parent == leaf->parent->parent->right) {
                    temp = leaf->parent->parent->left;

                    if(Color(temp) == RBTNodeColor::Red) {
                        temp->setColor(RBTNodeColor::Black);
                        leaf->parent->setColor(RBTNodeColor::Black);
                        leaf->parent->parent->setColor(RBTNodeColor::Red);
                        leaf = leaf->parent->parent;
                    } else {
                        if(leaf == leaf->parent->left) {
                            leaf = leaf->parent;
                            this->rightRotate(leaf);
                        }

                        leaf->parent->setColor(RBTNodeColor::Black);
                        leaf->parent->parent->setColor(RBTNodeColor::Red);
                        this->leftRotate(leaf->parent->parent);
                    }
                } 
                // left subtree
                else {
                    temp = leaf->parent->parent->right;

                    if(Color(temp) == RBTNodeColor::Red) {
                        temp->setColor(RBTNodeColor::Black);
                        leaf->parent->setColor(RBTNodeColor::Black);
                        leaf->parent->parent->setColor(RBTNodeColor::Red);
                        leaf = leaf->parent->parent;
                    } else {
                        if(leaf == leaf->parent->right) {
                            leaf = leaf->parent;
                            this->leftRotate(leaf);
                        }

                        leaf->parent->setColor(RBTNodeColor::Black);
                        leaf->parent->parent->setColor(RBTNodeColor::Red);
                        this->rightRotate(leaf->parent->parent);
                    }
                }

                // reached head of tree
                if(leaf == this->root) break;
            }

            // root is always colored black
            this->root->setColor(RBTNodeColor::Black);
        }

        /**
         * Fixes the red-black tree by recoloring (and possibly rotating) nodes starting with a
         * particular node which was modified as part of a removal operation.
         */
        void repairPostDelete(Leaf *x) {
            Leaf *s;
            while(x && x != this->root && Color(x) == RBTNodeColor::Black) {
                if(x == x->parent->left) {
                    s = x->parent->right;

                    if(Color(s) == RBTNodeColor::Red) {
                        s->setColor(RBTNodeColor::Black);
                        x->parent->setColor(RBTNodeColor::Red);
                        this->leftRotate(x->parent);

                        s = x->parent->right;
                    }

                    if(Color(s->left) == RBTNodeColor::Black && 
                       Color(s->right) == RBTNodeColor::Black) {
                        s->setColor(RBTNodeColor::Red);
                        x = x->parent;
                    } else {
                        if(Color(s->right) == RBTNodeColor::Black) {
                            s->left->setColor(RBTNodeColor::Black);
                            s->setColor(RBTNodeColor::Red);
                            this->rightRotate(s);

                            s = x->parent->right;
                        } 

                        s->setColor(Color(x->parent));
                        x->parent->setColor(RBTNodeColor::Black);
                        s->right->setColor(RBTNodeColor::Black);
                        this->leftRotate(x->parent);

                        x = this->root;
                    }
                } else {
                    s = x->parent->left;
                    if(Color(s) == RBTNodeColor::Red) {
                        s->setColor(RBTNodeColor::Black);
                        x->parent->setColor(RBTNodeColor::Red);
                        this->rightRotate(x->parent);

                        s = x->parent->left;
                    }

                    if(Color(s->right) == RBTNodeColor::Black &&
                       Color(s->right) == RBTNodeColor::Black) {
                        s->setColor(RBTNodeColor::Red);
                        x = x->parent;
                    } else {
                        if(Color(s->left) == RBTNodeColor::Black) {
                            s->right->setColor(RBTNodeColor::Black);
                            s->setColor(RBTNodeColor::Red);
                            this->leftRotate(s);

                            s = x->parent->left;
                        } 

                        s->setColor(Color(x->parent));
                        x->parent->setColor(RBTNodeColor::Black);
                        s->left->setColor(RBTNodeColor::Black);
                        this->rightRotate(x->parent);

                        x = this->root;
                    }
                } 
            }

            x->setColor(RBTNodeColor::Black);
        }

    protected:
        /**
         * Inserts the given node at the given subtree. Once the node has been inserted, recolor
         * the changed subtree so the tree is properly balanced.
         */
        bool insert(const KeyType key, Leaf *leaf, Leaf* &root, Leaf *parent = nullptr) override {
            // perform insertion
            if(!BST::insert(key, leaf, root, parent)) {
                return false;
            }

            // newly inserted nodes are always red...
            leaf->setColor(RBTNodeColor::Red);

            // if the node was inserted as the root, color black
            if(!leaf->parent) {
                leaf->setColor(RBTNodeColor::Black);
                return true;
            }
            // if no grandparent, bail out
            if(!leaf->parent->parent) return true;

            // repair tree
            this->repairPostInsert(leaf);
            return true;
        }

        /**
         * Search the given subtree for an object with the specified key and removes it. After the
         * object has been removed, the changed subtree(s) are recolored and the tree rebalanced.
         */
        /*bool remove(const KeyType key, Leaf *subtree, Leaf *parent) override {
            // find node with the key
            Leaf *toRemove = nullptr;

            while(subtree) {
                // the key matches
                if(subtree->getKey() == key) {
                    toRemove = subtree;
                    break;
                }

                // recurse down the appropriate side
                if(subtree->getKey() <= key) {
                    subtree = subtree->right;
                } else {
                    subtree = subtree->left;
                }
            }

            if(!toRemove) return false;

            // actually delete the node
            Leaf *toFix; // node which to start recoloring at
            Leaf *y = toRemove;
            auto yColor = Color(y);

            // no left child
            if(!toRemove->left && toRemove->right) {
                toFix = toRemove->right;
                this->swap(toRemove, toRemove->right);
            } 
            // no right child
            else if(toRemove->left && !toRemove->right) {
                toFix = toRemove->left;
                this->swap(toRemove, toRemove->left);
            }
            // have two children
            else if(toRemove->left && toRemove->right) {
                // find min node on right side (it will replace this item)
                y = this->findMin(toRemove->right);
                yColor = Color(y);
                toFix = y->right;

                if(y->parent == toRemove) {
                    if(toFix) toFix->parent = y;
                } else {
                    this->swap(y, y->right);
                    y->right = toFix ? toFix->right : nullptr;
                    if(y->right) y->right->parent = y;
                }

                this->swap(toFix, y);
                y->left = toFix ? toFix->left : nullptr;
                if(y->left) y->left->parent = y;
                y->setColor(Color(toFix));
            }
            // no children
            else {
                toFix = toRemove->parent;
                if(toRemove->parent->left == toRemove) {
                    toRemove->parent->left = nullptr;
                } else {
                    toRemove->parent->right = nullptr;
                }
            }

            // remove node
            //delete toRemove;
            log("removed: %p (%p)", toRemove, toFix);

            // if the removed node was black, recolor tree
            if(yColor == RBTNodeColor::Black) {
                this->repairPostDelete(toFix);
            }

            return true;
        }*/
};
}

#endif
