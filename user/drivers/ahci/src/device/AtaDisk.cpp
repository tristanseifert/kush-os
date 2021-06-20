#include "AtaDisk.h"
#include "AtaCommands.h"
#include "Controller.h"
#include "Port.h"

#include "Log.h"
#include "util/String.h"

#include <cstdint>
#include <cstring>
#include <mpack/mpack.h>
#include <driver/DrivermanClient.h>

/**
 * Initializes the ATA disk. We'll start by requesting identification from the drive to figure
 * out its parameters.
 */
AtaDisk::AtaDisk(const std::shared_ptr<Port> &port) : Device(port) {
    // allocate the response buffer
    int err;
    err = libdriver::ScatterGatherBuffer::Alloc(kSmallBufSize, this->smallBuf);
    if(err) {
        this->status = err;
        return;
    }

    // send the ATA IDENTIFY DEVICE command
    using namespace std::placeholders;
    err = port->submitAtaCommand(AtaCommand::Identify, this->smallBuf,
            std::bind(&AtaDisk::handleIdentifyResponse, this, _1));
    if(err) {
        this->status = err;
        return;
    }
}

/**
 * Helper method to allocate an ATA disk
 */
int AtaDisk::Alloc(const std::shared_ptr<Port> &port, std::shared_ptr<AtaDisk> &outDisk) {
    std::shared_ptr<AtaDisk> disk(new AtaDisk(port));
    if(!disk->getStatus()) {
        outDisk = disk;
    }
    return disk->getStatus();
}

/**
 * Cleans up resources belonging to the ATA disk.
 */
AtaDisk::~AtaDisk() {

}

/**
 * Invalidates the device; if we're still attached to a port, we'll detach from that port and
 * have it probe again.
 */
void AtaDisk::invalidate() {
    Warn("TODO: implement %s", __PRETTY_FUNCTION__);
}


/**
 * Handles the response to the initial identify device response.
 */
void AtaDisk::handleIdentifyResponse(const Port::CommandResult &res) {
    auto port = this->port.lock();
    if(!port) {
        Abort("Port for device %p disappeared during identify!", this);
    }

    if(!res.isSuccess()) {
        Warn("%s Identify for port %u failed: status %02x", "ATA", port->getPortNumber(),
                res.getAtaError());
        return this->invalidate();
    }

    // extract all the info we need from the identify response
    auto span = static_cast<std::span<std::byte>>(*this->smallBuf);
    this->identifyDetermineSize(span);
    this->identifyExtractStrings(span);

    // the device is ready for use :D
    this->registerDisk();
}

/**
 * Extracts identifier strings from the identify command response.
 */
void AtaDisk::identifyExtractStrings(const std::span<std::byte> &span) {
    auto model = std::string(reinterpret_cast<const char *>(span.subspan(54, 40).data()), 40);
    util::ConvertAtaString(model);
    util::TrimTrailingWhitespace(model);
    this->model = model;

    auto serial = std::string(reinterpret_cast<const char *>(span.subspan(20, 20).data()), 20);
    util::ConvertAtaString(serial);
    util::TrimTrailingWhitespace(serial);
    this->serial = serial;

    auto fw = std::string(reinterpret_cast<const char *>(span.subspan(46, 8).data()), 8);
    util::ConvertAtaString(fw);
    util::TrimTrailingWhitespace(fw);
    this->firmwareVersion = fw;

    Trace("Model '%s', serial '%s'. Firmware %s", model.c_str(), serial.c_str(), fw.c_str());
}

/**
 * Figure out the number of sectors on the device, as well as the size of each sector in bytes:
 * this can be a little bit tricky to get right.
 *
 * First, we'll check in the identify data word 83 (command and feature sets supported) to see if
 * the device supports 48-bit addressing. We'll also read out the dword at words 60-61, which is
 * the number of addressable sectors for 28-bit commands. 
 *
 * If 48-bit addressing support is available, the qword at words 100-103 is mandatory and will
 * contain one greater than the maximum accessible LBA.
 */
void AtaDisk::identifyDetermineSize(const std::span<std::byte> &span) {
    uint16_t temp;

    // test for support for 48-bit addressing
    memcpy(&temp, span.subspan(166, 2).data(), sizeof(temp));
    const bool supports48Bit = (temp & (1 << 10));

    Trace("48 bit support: %d", supports48Bit);

    // read out the appropriate size value
    if(supports48Bit) {
        uint64_t largeSize;
        memcpy(&largeSize, span.subspan(200, 8).data(), sizeof(largeSize));

        this->numSectors = largeSize - 1;
    } else {
        uint32_t smallSize;
        memcpy(&smallSize, span.subspan(120, 4).data(), sizeof(smallSize));

        this->numSectors = smallSize - 1;
    }

    Trace("Have %lu sectors at %lu bytes each", this->numSectors, this->sectorSize);
}



/**
 * Registers the disk in the driver forest.
 */
void AtaDisk::registerDisk() {
    auto port = this->port.lock();

    // prepare the properties and the path
    std::vector<std::byte> info, connection;
    this->serializeInfoData(info, port);
    this->serializeConnectionData(connection, port);

    const auto controllerPath = port->getController()->getForestPath();
    std::string name("AtaDisk@");
    name.append(std::to_string(port->getPortNumber()));

    // register that dude
    auto rpc = libdriver::RpcClient::the();
    this->forestPath = rpc->AddDevice(controllerPath, name);

    rpc->SetDeviceProperty(this->forestPath, kInfoPropertyName, info);
    rpc->SetDeviceProperty(this->forestPath, kConnectionPropertyName, connection);

    rpc->StartDevice(this->forestPath);

    Success("ATA disk registered as %s (%lu MB)", this->forestPath.c_str(), (this->sectorSize * this->numSectors) / 1000 / 1000);
}

/**
 * Serializes information about this device.
 */
void AtaDisk::serializeInfoData(std::vector<std::byte> &out, const std::shared_ptr<Port> &port) {
    char *data;
    size_t size;

    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 6);

    // write out the size of the disk
    mpack_write_cstr(&writer, "sectorSize");
    mpack_write_u32(&writer, this->sectorSize);
    mpack_write_cstr(&writer, "sectors");
    mpack_write_u64(&writer, this->numSectors);
    // write its location
    mpack_write_cstr(&writer, "ahciPort");
    mpack_write_u8(&writer, port->getPortNumber());
    // device strings
    mpack_write_cstr(&writer, "model");
    mpack_write_cstr(&writer, this->model.c_str());
    mpack_write_cstr(&writer, "serial");
    mpack_write_cstr(&writer, this->serial.c_str());
    mpack_write_cstr(&writer, "fwver");
    mpack_write_cstr(&writer, this->firmwareVersion.c_str());

    // copy to output buffer
    mpack_finish_map(&writer);

    auto status = mpack_writer_destroy(&writer);
    if(status != mpack_ok) {
        Warn("%s failed: %d", "mpack_writer_destroy", status);
        return;
    }

    out.resize(size);
    out.assign(reinterpret_cast<std::byte *>(data), reinterpret_cast<std::byte *>(data + size));
    free(data);
}

/**
 * Serializes the information on how to connect to this device. This consists of its unique id in
 * the disk RPC service.
 */
void AtaDisk::serializeConnectionData(std::vector<std::byte> &out, const std::shared_ptr<Port> &port) {
    char *data;
    size_t size;

    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    mpack_start_map(&writer, 2);

    // write the IDs
    mpack_write_cstr(&writer, "id");
    mpack_write_u32(&writer, -1);

    // write port name
    mpack_write_cstr(&writer, "port");
    mpack_write_u64(&writer, -1);

    // copy to output buffer
    mpack_finish_map(&writer);

    auto status = mpack_writer_destroy(&writer);
    if(status != mpack_ok) {
        Warn("%s failed: %d", "mpack_writer_destroy", status);
        return;
    }

    out.resize(size);
    out.assign(reinterpret_cast<std::byte *>(data), reinterpret_cast<std::byte *>(data + size));
    free(data);
}

