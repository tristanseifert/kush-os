#include "Device.h"
#include "Forest.h"
#include "DriverInstance.h"
#include "Log.h"

#include "db/Driver.h"
#include "db/DriverDb.h"
#include "util/String.h"

#include <stdexcept>

/**
 * Splits a comma separated list of driver names into an ordered list. The list may contain only a
 * single entry, but it may not be empty.
 *
 * @param str Parameter string to split
 * @param out Vector to append the results to
 */
void Device::SplitDriverName(const std::string_view &str, std::vector<std::string> &out) {
    if(str.empty()) {
        Abort("Invalid driver name");
    }

    // early exit if no comma
    if(str.find(kDriverNameSeparator) == std::string_view::npos) {
        out.emplace_back(str);
        return;
    }

    // tokenization time!!
    size_t start{0}, current{0};
    while((current = str.find_first_of(kDriverNameSeparator, start)) != std::string_view::npos) {
        out.emplace_back(str.substr(start, current));
        start = current+1;
    }
    if(start < str.length()) { // leftover
        out.emplace_back(str.substr(start, current));
    }
}

/**
 * Removes the forest reference.
 */
void Device::willDeforest(const std::shared_ptr<Forest::Leaf> &leaf) {
    if(this->leaf.lock() != leaf) {
        Abort("Invalid deforestation leaf");
    }

    this->leaf.reset();
}

/**
 * Updates the leaf we're owned by.
 */
void Device::didReforest(const std::shared_ptr<Forest::Leaf> &leaf) {
    this->leaf = leaf;
}



/**
 * If the device is in the forest, return its path.
 */
std::optional<std::string> Device::getPath() {
    auto leaf = this->leaf.lock();
    if(leaf) {
        return leaf->getPath();
    }
    return std::nullopt;
}

/**
 * Tries to find a driver for this device.
 *
 * @return Whether a driver was found and loaded.
 */
bool Device::findAndLoadDriver() {
    DriverDb::MatchInfo info;
    auto sThis = this->shared_from_this();

    // find the driver
    auto driver = DriverDb::the()->findDriver(sThis, &info);
    if(!driver) return false;

    // cool! start it (this will set our instance ptr)
    driver->start(sThis);

    return true;
}
