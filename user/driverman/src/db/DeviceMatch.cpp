#include "DeviceMatch.h"
#include "Log.h"
#include "forest/Device.h"

#include <vector>

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

