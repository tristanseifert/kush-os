#include "Forest.h"

#include "Log.h"

Forest *Forest::gShared = nullptr;

/**
 * Initializes the forest.
 */
Forest::Forest() {
    // create the root node
    this->root = std::make_shared<Leaf>();
}
