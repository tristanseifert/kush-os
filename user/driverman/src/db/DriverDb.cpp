#include "DriverDb.h"
#include "Driver.h"
#include "Log.h"

DriverDb *DriverDb::gShared = nullptr;



/**
 * Load the driver database.
 *
 * TODO: actually load it; we just create dummy entries for now
 */
DriverDb::DriverDb() {
    // bus drivers
    auto pciRoot = std::make_shared<Driver>("/sbin/pcisrv");
    pciRoot->addMatch(static_cast<libdriver::DeviceMatch *>(
                new libdriver::DeviceNameMatch("AcpiPciRootBridge")));
    this->addDriver(pciRoot);
}


/**
 * Attempts to find a driver that matches any of the given match constraints.
 *
 * @return Driver if found, otherwise `nullptr.`
 */
std::shared_ptr<Driver> DriverDb::findDriver(const std::span<libdriver::DeviceMatch *> &matches) {
    std::shared_ptr<Driver> ret = nullptr;

    // scan all drivers
    this->driversLock.lock_shared();

    for(auto &[id, driver] : this->drivers) {
        if(driver->test(matches)) {
            ret = driver;
        }
    }

    // done
    this->driversLock.unlock_shared();
    return ret;
}

/**
 * Register new driver.
 *
 * @return ID of the newly inserted driver.
 */
uintptr_t DriverDb::addDriver(std::shared_ptr<Driver> &driver) {
    // acquire lock, get id
    this->driversLock.lock();

again:;
    auto id = this->nextId++;
    if(!id) {
        goto again;
    }

    // inscrete
    this->drivers.emplace(id, driver);

    // clean up
    this->driversLock.unlock();
    return id;
}

/**
 * Removes the driver with the given id.
 *
 * @return Whether such a driver exists and was removed.
 */
bool DriverDb::removeDriver(const uintptr_t id) {
    this->driversLock.lock();
    const auto nRemoved = this->drivers.erase(id);
    this->driversLock.unlock();

    return (nRemoved == 1);
}
