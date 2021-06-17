#include "DeviceMatch.h"
#include "Log.h"
#include "forest/Device.h"

#include <vector>
#include <mpack/mpack.h>

/**
 * Checks if the given device specifies our name anywhere in its list of driver names. Depending
 * on its index in the driver name list, we'll apply a negative offset to the base priority value.
 */
bool DeviceNameMatch::supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) {
    const auto &names = dev->getDriverNames();

    for(size_t i = 0; i < names.size(); i++) {
        // trim off the aux info if needed
        std::string dn = names[i];
        if(dn.find_first_of('@') != std::string::npos) {
            dn = dn.substr(0, dn.find_first_of('@'));
        }

        if(dn == this->name) {
            outPriority = this->priority - static_cast<int>(i);
            return true;
        }
    }

    // if we get here, no name matched
    return false;
}



const std::string PciDeviceMatch::kPciExpressInfoPropertyName{"pcie.info"};

/**
 * Adds a new vid/pid match to the match object.
 */
void PciDeviceMatch::addVidPidMatch(const uint16_t vid, const std::optional<uint16_t> pid,
        const std::optional<int> priority) {
    VidPidMatch match{vid, pid, priority};
    this->vidPid.emplace_back(match);
}

/**
 * If the given device is a PCI or PCI Express device, attempt to match against it.
 */
bool PciDeviceMatch::supportsDevice(const std::shared_ptr<Device> &dev, int &outPriority) {
    bool success{false};

    // decode the PCI info
    mpack_tree_t tree;
    std::vector<std::byte> info;

    if(dev->hasProperty(kPciExpressInfoPropertyName)) {
        info = dev->getProperty(kPciExpressInfoPropertyName);
        mpack_tree_init_data(&tree, reinterpret_cast<const char *>(info.data()), info.size());
    } else {
        // not a PCI device
        return false;
    }


    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    const uint8_t classId = mpack_node_u8(mpack_node_map_cstr(root, "class")),
               subclassId = mpack_node_u8(mpack_node_map_cstr(root, "subclass"));
    const uint16_t    vid = mpack_node_u16(mpack_node_map_cstr(root, "vid")),
                      pid = mpack_node_u16(mpack_node_map_cstr(root, "pid"));

    if(kLogMatch) Trace("Match against %04x:%04x, class %02x:%02x (expected %02x %02x)", vid, pid,
            classId, subclassId, this->classId.value_or(-1), this->subclassId.value_or(-1));

    // clean up the msgpack decoder
    auto status = mpack_tree_destroy(&tree);
    if(status != mpack_ok) {
        Warn("%s failed: %d", "mpack_tree_destroy", status);
    }

    // match class id if needed
    if(this->classId) {
        if(*this->classId != classId) return false;

        outPriority = this->priority;
        success = true;
    }
    // match subclass id if needed
    if(this->subclassId) {
        if(*this->subclassId != subclassId) return false;

        outPriority = this->priority;
        success = true;
    }

    // if there's no VIDs to match against we can exit with success
    if(this->vidPid.empty()) return success;

    // find a vid/pid match, even if we've already succeeded, to catch priority override
    for(const auto &m : this->vidPid) {
        // check vid (always required for match)
        if(m.vid != vid) continue;
        // if pid is provided, match it
        if(m.pid && *m.pid != pid) continue;

        // we matched! update priority if provided
        if(m.priority) outPriority = *m.priority;
        success = true;

        return true;
    }

    // if we get here, no device matched :(
    return success;
}

