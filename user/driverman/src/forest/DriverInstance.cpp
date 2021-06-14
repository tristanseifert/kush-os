#include "DriverInstance.h"
#include "Device.h"
#include "Forest.h"
#include "Log.h"


/**
 * Create a driver instance object.
 */
DriverInstance::DriverInstance(const std::shared_ptr<Driver> &_driver, const uintptr_t _task) :
    driver(_driver), taskHandle(_task) {

}
