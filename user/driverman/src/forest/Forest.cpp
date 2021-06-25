#include "Forest.h"
#include "Device.h"

#include "Log.h"
#include "util/String.h"

#include <stdexcept>
#include <sstream>

Forest *Forest::gShared = nullptr;

/**
 * Initializes the forest.
 */
Forest::Forest() {
    // create the root node
    this->root = std::make_shared<Leaf>();
}

/**
 * Insert the given device at the given path.
 *
 * @param path Parent node to insert the device under
 * @param dev Device to insert
 * @param outPath Path that the device was inserted at
 * @param loadDriver If set, whether we should immediately attempt to find a driver for the device
 *
 * @return Whether the node was inserted, e.g. whether the given path of its parent is valid.
 */
bool Forest::insertDevice(const std::string_view &path, const std::shared_ptr<Device> &dev,
        std::string &outPath, const bool loadDriver) {
    std::string desiredName;

    // locate parent
    std::shared_ptr<Leaf> parent;
    if(!this->find(path, parent)) {
        return false;
    }

    // check if the desired name would conflict
    desiredName = dev->getPrimaryName();
    for(const auto &child : *parent) {
        if(child->name == desiredName) {
            Abort("TODO: handle naming conflicts!");
        }
    }

    // if not, create the object
    auto leaf = std::make_shared<Leaf>(desiredName, parent);
    parent->children.push_back(leaf);

    UpdateLeafDev(dev, leaf);

    // and store its path
    outPath = leaf->getPath();

    // try matching a driver if not already assigned
    if(loadDriver && !dev->hasDriver()) {
        dev->findAndLoadDriver();
    }

    return true;
}

/**
 * Finds a device at the given path.
 */
std::shared_ptr<Device> Forest::getDevice(const std::string_view &path) {
    std::shared_ptr<Leaf> leaf;
    if(!this->find(path, leaf)) return nullptr;
    return leaf->device;
}



/**
 * Iterates the tree in a breadth-first fashion to start drivers for devices that do not yet have
 * any drivers associated with them.
 */
void Forest::startDeviceDrivers() {
    this->startDriversOn(this->root);
}

/**
 * Starts drivers for all devices attached to this leaf, before recursing to its children.
 */
void Forest::startDriversOn(const std::shared_ptr<Leaf> &leaf) {
    if(leaf->device) {
        const auto &dev = leaf->device;
        if(!dev->hasDriver()) {
            dev->findAndLoadDriver();
        }
    }

    // recurse to children
    for(const auto &c : *leaf) {
        this->startDriversOn(c);
    }
}

/**
 * Searches the forest for a node with the given path.
 */
bool Forest::find(const std::string_view &_path, std::shared_ptr<Forest::Leaf> &outLeaf) {
    // empty string or just a separator indicate the root of the tree
    if(_path.empty() || _path == kPathSeparator) {
        outLeaf = this->root;
        return true;
    }

    // we gotta iterate the path now :D
    std::string path(_path);
    auto leaf = this->root;

    if(path[0] == '/') {
        path = path.substr(1);
    }

    std::stringstream stream{path};
    std::string name;

    while(std::getline(stream, name, '/')) {
        // search the leaf's children for one with this name
        for(const auto &child : *leaf) {
            if(child->name == name) {
                leaf = child;
                goto mcdonalds;
            }
        }
        return false;

        // check next component
mcdonalds:;
    }

    // return the leaf
    outLeaf = leaf;
    return true;
}

/**
 * Updates the device associated with a leaf in the forest. The leaf association in the device is
 * updated accordingly.
 */
void Forest::UpdateLeafDev(const std::shared_ptr<Device> &device,
        const std::shared_ptr<Forest::Leaf> &leaf) {
    // remove old device association
    if(leaf->device) {
        leaf->device->willDeforest(leaf);
        leaf->device.reset();
    }

    // add to new leaf
    leaf->device = device;
    device->didReforest(leaf);
}



/**
 * Get the path of the given leaf. This works by traversing the `parent` pointer until we get to
 * the root of the forest.
 */
std::string Forest::Leaf::getPath() const {
    std::string path = this->name;
    path.insert(0, kPathSeparator);

    std::shared_ptr<Leaf> parent = this->parent.lock();
    while(parent) {
        // only the root should have an empty name
        if(!parent->name.empty()) {
            path.insert(0, parent->name);
            path.insert(0, kPathSeparator);
        }

        parent = parent->parent.lock();
    }

    return path;
}
