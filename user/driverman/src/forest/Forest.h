#ifndef FOREST_FOREST_H
#define FOREST_FOREST_H

#include <cassert>
#include <list>
#include <memory>

/**
 * The forest is a tree-like structure in which all drivers are registered. This builds up a sort
 * of dependency and provides a clear path of what devices are on what bus, for example.
 *
 * In general, you should never hold a strong reference to objects in the forest above your level;
 * this will create retain cycles.
 */
class Forest {
    public:
        /// Initialize the shared instance of the forest.
        static void init() {
            assert(!gShared);
            gShared = new Forest;
        }

        /// Get the global instance of the forest
        static Forest *the() {
            return gShared;
        }

    private:
        Forest();

    private:
        struct Leaf {
            virtual ~Leaf() = default;

            /// Starting iterator
            auto begin() {
                return this->children.begin();
            }
            /// Ending iterator
            auto end() {
                return this->children.end();
            }

            /**
             * All children of this node in the tree
             */
            std::list<std::shared_ptr<Leaf>> children;
        };

    private:
        static Forest *gShared;

    private:
        /// root element of the tree
        std::shared_ptr<Leaf> root;
};

#endif
