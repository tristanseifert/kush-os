#include "DriverDb.h"
#include "DbParser.h"
#include "Driver.h"
#include "Log.h"

#include <queue>

DriverDb *DriverDb::gShared{nullptr};



/**
 * Load the driver database. Since this is called during early driverman setup when we're running
 * out of the RAM disk, this will be the driver listing for the drivers that are required to get
 * the filesystem access going.
 *
 * Once we can read the boot filesystem, we'll read a second driver database file that will
 * supplant (but not replace; devices can still be matched by them, but the paths they reference
 * must also exist on the real filesystem) the init drivers.
 */
DriverDb::DriverDb() {
    DbParser p;

    if(!p.parse(kBootDbPath, this)) {
        Abort("Failed to load initial driver database");
    }
}


/**
 * Finds a driver that can match to the given device. If there are multiple drivers that match,
 * the one with the highest priority is returned.
 *
 * @param device Device to match against
 * @param outDriver Optional pointer to a match info structure that will be filled in on match
 *
 * @return The appropriate driver, or `nullptr` if none were found.
 */
std::shared_ptr<Driver> DriverDb::findDriver(const std::shared_ptr<Device> &device,
        MatchInfo *outDriver) {
    std::priority_queue<MatchInfo> mi;

    // check all drivers to see if they match
    this->driversLock.lock_shared();

    for(const auto &[key, driver] : this->drivers) {
        int score{0};
        if(driver->test(device, score)) {
            mi.emplace(driver, score);
        }
    }

    this->driversLock.unlock_shared();

    // select the highest priority match
    if(mi.empty()) return nullptr;

    auto &best = mi.top();
    if(outDriver) *outDriver = best;
    return best.driver;
}


/**
 * Register new driver.
 *
 * @return ID of the newly inserted driver.
 */
uintptr_t DriverDb::addDriver(const std::shared_ptr<Driver> &driver) {
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
