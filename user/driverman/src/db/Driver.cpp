#include "Driver.h"
#include "DeviceMatch.h"
#include "Log.h"

#include "forest/DriverInstance.h"
#include "forest/Device.h"

#include <rpc/task.h>
#include <driver/base85.h>

#include <algorithm>
#include <system_error>
#include <new>

/**
 * Creates a driver whose binary is located at the given path.
 */
Driver::Driver(const std::string &_path) : path(_path) {

}

/**
 * Delete all match objects.
 */
Driver::~Driver() {
    for(auto ptr : this->matches) {
        delete ptr;
    }
}


/**
 * Adds a match object.
 */
void Driver::addMatch(DeviceMatch *match) {
    this->matches.push_back(match);
}

/**
 * Tests whether we can match to the device. This will query every match descriptor to see if it
 * matches, and and returns the highest priority value returned by them all.
 */
bool Driver::test(const std::shared_ptr<Device> &dev, int &outPriority) {
    int priority{0};

    // bail if no match descriptors
    if(this->matches.empty()) return false;

    // we must match all of the descriptors, but return the highest priority
    if(this->mustMatchAll) {
        for(auto m : this->matches) {
            int temp{0};

            if(!m->supportsDevice(dev, temp)) return false;
            priority = std::max(priority, temp);
        }

        outPriority = priority;
        return true;
    }
    // any match is ok; select the highest priority one
    else {
        bool matched{false};

        for(auto m : this->matches) {
            int temp{0};

            if(!m->supportsDevice(dev, temp)) continue;
            priority = std::max(priority, temp);
            matched = true;
        }

        if(matched) outPriority = priority;
        return matched;
    }
}



/**
 * Either starts a new instance of the driver, or connects to an existing driver and notifies it
 * that a device has been added.
 *
 * Regardless, the passed in device will have a driver instance object assigned.
 */
void Driver::start(const std::shared_ptr<Device> &dev) {
    std::shared_ptr<DriverInstance> inst;

    // first instance OR always launch a new instance
    if(this->instances.empty() || this->alwaysLaunchNew) {
        inst = this->makeInstance(dev);
        this->instances.push_back(inst);
    }
    // notify existing instance
    else {
        Abort("%s (2+ instance) unimplemented", __PRETTY_FUNCTION__);
    }

    // update the device
    dev->setDriver(inst);
}

/**
 * Creates a new driver instance object for the given device.
 */
std::shared_ptr<DriverInstance> Driver::makeInstance(const std::shared_ptr<Device> &dev) {
    int err;
    const std::string devPath = dev->getPath().value();

    // create the argument string
    const char *params[3] = {
        // path of the executable
        this->path.c_str(),
        // path of the device in forest
        devPath.c_str(),
        // end of list
        nullptr,
    };

    Trace("Launching driver %s", this->path.c_str());

    // TODO: create the driver port

    // launch the task
    uintptr_t handle;
    err = RpcTaskCreate(this->path.c_str(), params, &handle);

    if(err) {
        throw std::system_error(err, std::system_category(), "RpcTaskCreate");
    }

    // create the instance object
    return std::make_shared<DriverInstance>(this->shared_from_this(), handle);
}
