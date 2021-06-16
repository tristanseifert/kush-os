#include "BusRegistry.h"
#include "Log.h"

#include "bus/pcie/PciExpressBus.h"

BusRegistry *BusRegistry::gShared{nullptr};

/**
 * Initializes the shared instance.
 */
void BusRegistry::init() {
    if(gShared) Abort("Cannot reinitialize bus registry");
    gShared = new BusRegistry;
}

/**
 * Adds a device to the bus registry.
 */
void BusRegistry::add(const std::shared_ptr<PciExpressBus> &bus) {
    // add to list
    this->pcie.emplace_back(bus);

    // add to existing segment
    if(this->segments.contains(bus->getSegment())) {
        auto &s = this->segments.at(bus->getSegment());
        s.busses.emplace_back(bus);
    }
    // insert new segment
    else {
        Segment s;
        s.busses.emplace_back(bus);

        this->segments.emplace(bus->getSegment(), std::move(s));
    }
}


/**
 * Scans all busses.
 */
size_t BusRegistry::scanAll() {
    size_t devices{0};

    for(const auto &bus : this->pcie) {
        bus->scan();
        devices += bus->getDevices().size();
    }

    return devices;
}


/**
 * Gets the bus that is responsible for handling the device at the given address.
 */
std::shared_ptr<PciExpressBus> BusRegistry::get(const DeviceAddress &addr) const {
    // get the segment
    if(!this->segments.contains(addr.segment)) return nullptr;
    const auto &seg = this->segments.at(addr.segment);

    // and then get the bus on that segment
    return seg.getBusFor(addr);
}

/**
 * Gets the bus that is responsible for handling the device at the given address on this segment.
 */
std::shared_ptr<PciExpressBus> BusRegistry::Segment::getBusFor(const DeviceAddress &addr) const {
    for(const auto &bus : this->busses) {
        if(bus->containsDevice(addr)) return bus;
    }
    return nullptr;
}
