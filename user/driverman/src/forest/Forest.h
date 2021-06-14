#ifndef FOREST_FOREST_H
#define FOREST_FOREST_H

#include <cassert>
#include <list>
#include <memory>
#include <string>
#include <string_view>

class Device;

/**
 * The forest is a tree-like structure in which all devices are registered. This builds up a sort
 * of dependency and provides a clear path of what devices are on what bus, for example. Each
 * devie can then be claimed by a driver through a matching process.
 *
 * In general, you should never hold a strong reference to objects in the forest above your level;
 * this will create retain cycles.
 */
class Forest {
    friend class Device;

    /// Separator string for device paths
    constexpr static const std::string_view kPathSeparator{"/"};

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

        /// Inserts a new device node.
        bool insertDevice(const std::string_view &path, const std::shared_ptr<Device> &device,
                std::string &outPath, const bool loadDriver = true);
        /// Gets the device associated with the node at the given path.
        std::shared_ptr<Device> getDevice(const std::string_view &path);

    private:
        Forest();

    private:
        struct Leaf {
            Leaf() = default;
            Leaf(const std::string &_name, const std::shared_ptr<Leaf> &_parent) : name(_name),
                parent(_parent) {}
            virtual ~Leaf() = default;

            /// Starting iterator
            auto begin() {
                return this->children.begin();
            }
            /// Ending iterator
            auto end() {
                return this->children.end();
            }

            /// Return the string path of this leaf by traversing upwards.
            std::string getPath() const;

            /// String name of this device
            std::string name;
            /// Device assigned to this node in the tree
            std::shared_ptr<Device> device{nullptr};

            /// All children of this node in the tree
            std::list<std::shared_ptr<Leaf>> children;

            /// Parent leaf (or `null` if root)
            std::weak_ptr<Leaf> parent;
        };

    private:
        /// Finds the device at a particular path
        bool find(const std::string_view &, std::shared_ptr<Forest::Leaf> &);

        /// Associates the given device to the given leaf.
        static void UpdateLeafDev(const std::shared_ptr<Device> &, const std::shared_ptr<Leaf> &);

    private:
        static Forest *gShared;

    private:
        /// root element of the tree
        std::shared_ptr<Leaf> root;
};

#endif
