#include "Server.h"
#include "Log.h"

#include "db/DriverDb.h"
#include "forest/Forest.h"
#include "forest/Device.h"

#include "util/String.h"

#include <driver/DrivermanClient.h>
#include <rpc/rt/ServerPortRpcStream.h>

RpcServer *RpcServer::gShared{nullptr};

/**
 * Initializes the global RPC server instance. This will create the appropriate listening IO
 * object as well on the driverman port.
 */
void RpcServer::init() {
    auto strm = std::make_shared<rpc::rt::ServerPortRpcStream>(kRpcEndpointName);
    gShared = new RpcServer(strm);
}



/**
 * Handles the addition of a new device to the tree.
 *
 * @param parent Path to the parent device under which to add this one; may be empty for the root
 * @param driverId A name or list of names of drivers to handle this device, in descending order
 *
 * @return Path to the inserted device, or an empty string on error
 */
std::string RpcServer::implAddDevice(const std::string &_parent, const std::string &driverId) {
    std::string devPath;

    // allow an empty string to stand for the root of the device tree
    auto parent = _parent;
    if(parent.empty()) {
        parent = "/";
    }

    // create device and insert it
    auto device = std::make_shared<Device>(driverId);
    const auto success = Forest::the()->insertDevice(parent, device, devPath);

    if(success) {
        return devPath;
    }

    return "";
}

/**
 * Sets a property (identified by its key) on the device. Properties are binary blobs; it's up to
 * the application to decide how to interpret them.
 *
 * @note If the key already exists, its existing value is overwritten.
 *
 * @param path Path of the device to set the property on
 * @param key Name of the property to set
 * @param data Data to set under this key; a zero byte value will delete the key.
 */
int32_t RpcServer::implSetDeviceProperty(const std::string &path, const std::string &key, const std::vector<std::byte> &data) {
    auto device = Forest::the()->getDevice(path);
    if(!device) {
        Warn("Failed to get device at '%s' to set property '%s'", path.c_str(), key.c_str());
        return Errors::NoSuchDevice;
    }

    if(kLogProperties) Trace("%s: Set %s = (%lu bytes)", path.c_str(), key.c_str(), data.size());
    if(data.empty()) {
        device->removeProperty(key);
    } else {
        device->setProperty(key, data);
    }

    return 0;
}

/**
 * Gets the value of a device property.
 *
 * @param path Path of the device to get the property from
 * @param key Key to retrieve the data of
 *
 * @return Data associated with the key, or an zero byte blob if not found.
 */
RpcServer::GetDevicePropertyReturn RpcServer::implGetDeviceProperty(const std::string &path,
        const std::string &key) {
    auto device = Forest::the()->getDevice(path);
    if(!device) {
        Warn("Failed to get device at '%s' to set property '%s'", path.c_str(), key.c_str());
        return { Errors::NoSuchDevice };
    }

    if(kLogProperties) Trace("%s: Get %s", path.c_str(), key.c_str());
    if(device->hasProperty(key)) {
        return {0, device->getProperty(key)};
    }
    return {0, {}};
}



/**
 * Starts the specified device.
 */
int32_t RpcServer::implStartDevice(const std::string &path) {
    auto device = Forest::the()->getDevice(path);
    if(!device) return Status::NoDevice;

    // load a driver if not already
    if(!device->hasDriver()) {
        device->findAndLoadDriver();
    }

    // start it
    return device->start();
}

/**
 * Stops the given device.
 */
int32_t RpcServer::implStopDevice(const std::string &path) {
    auto device = Forest::the()->getDevice(path);
    if(!device) return Status::NoDevice;

    // send stop request
    return device->stop();
}

/**
 * Handle notifications.
 */
extern "C" void __librpc__FileIoResetConnection();

int32_t RpcServer::implNotify(const std::string &path, uint64_t key) {
    using NK = libdriver::RpcClient::NoteKeys;

    // driverman internal notification
    if(path.empty()) {
        switch(key) {
            /**
             * Root fs has updated; read the updated driver db and re-probe.
             */
            case static_cast<uint64_t>(NK::RootFsUpdated): {
                __librpc__FileIoResetConnection();

                // reload the driver database and match any devices without a driver
                DriverDb::the()->loadFullDb();
                Forest::the()->startDeviceDrivers();
                break;
            }

            default:
                Warn("Unknown driverman notify key: $%p", key);
                break;
        }
    }
    else {
        Warn("Device notify (%s) %p not implemented!", path.c_str(), key);
        return Status::NoDevice;
    }

    return Status::Success;
}
