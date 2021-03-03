#ifndef LIBDRIVER_DRIVER_MSG_DISCOVERY_HPP
#define LIBDRIVER_DRIVER_MSG_DISCOVERY_HPP

#include "Messages.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace libdriver {
/**
 * Base type for all match structures
 */
struct DeviceMatch: public Message {
    virtual ~DeviceMatch() = default;

    /// Creates a device match structure from a node.
    static DeviceMatch * _Nullable fromNode(mpack_node_t &);

    /// Tests whether this device match structure and another match.
    virtual bool matches(DeviceMatch * _Nonnull) {
        return false;
    }
};

/**
 * Match on a driver/device based on its name only.
 */
struct DeviceNameMatch: public DeviceMatch {
    constexpr static const uint32_t kRpcType = 'DMN ';
    constexpr static const uint8_t kMatchType = 0x01;

    DeviceNameMatch() = default;
    DeviceNameMatch(const std::string &_name) : name(_name) {}

    /// Name of driver to match against
    std::string name;

    void serialize(mpack_writer_t * _Nonnull writer) override;
    void deserialize(mpack_node_t &root) override;
    const uint32_t getRpcType() const override {
        return kRpcType;
    }
    virtual bool matches(DeviceMatch * _Nonnull _in) override {
        if(_in->getRpcType() != kRpcType) return false;

        auto in = reinterpret_cast<DeviceNameMatch *>(_in);
        return (in->name == this->name);
    }
};

/**
 * Message indicating that a new device has been discovered and a driver should be loaded for it.
 *
 * These requests include one or more match structures -- the same ones used when loading drivers
 * and searching for initialized devices -- that the driver server uses to load the appropriate
 * driver.
 *
 * Additionally, the message may carry arbitrary auxiliary data, which is serialized and passed to
 * the driver when initializing the device.
 */
struct DeviceDiscovered: public Message {
    constexpr static const uint32_t kRpcType = 'DDSC';

    /// match structures
    std::vector<DeviceMatch * _Nonnull> matches;
    /// optional aux data
    std::vector<std::byte> aux;

    void serialize(mpack_writer_t * _Nonnull writer) override;
    void deserialize(mpack_node_t &root) override;

    virtual ~DeviceDiscovered();

    const uint32_t getRpcType() const override {
        return kRpcType;
    }

    private:
        /// when set, we need to deallocate pointers
        bool ownsPtrs = false;
};
}

#endif
