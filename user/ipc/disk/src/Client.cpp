#include "Client.h"

#include <cstdio>

#include <unistd.h>
#include <mpack/mpack.h>
#include <driver/DrivermanClient.h>
#include <rpc/rt/ClientPortRpcStream.h>

using namespace DriverSupport::disk;

/// Region of virtual memory space for disk metadata buffers
static uintptr_t kMappingRange[2] = {
    // start
    0x67800000000,
    // end
    0x67808000000,
};

/**
 * Allocates a new disk device, if the given forest path is a valid disk.
 */
int Disk::Alloc(const std::string_view &path, std::shared_ptr<Disk> &outDisk) {
    // get the disk RPC connection info property of this device
    auto driverman = libdriver::RpcClient::the();

    auto value = driverman->GetDeviceProperty(path, kConnectionPropertyName);
    if(value.empty()) {
        return Errors::InvalidPath;
    }

    // extract from it the port handle and disk id
    const auto [port, diskId] = DecodeConnectionInfo(value);
    if(!port || !diskId) {
        return Errors::InvalidConnectionInfo;
    }

    auto io = std::make_shared<rpc::rt::ClientPortRpcStream>(port);
    std::shared_ptr<Disk> ptr(new Disk(io, path, diskId));

    if(ptr->status) {
        return ptr->status;
    }

    outDisk = ptr;
    return 0;
}

/**
 * Decodes the connection info blob provided.
 *
 * @return A pair of the RPC port and disk id to service this disk.
 */
std::pair<uintptr_t, uint64_t> Disk::DecodeConnectionInfo(const std::span<std::byte> &d) {
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, reinterpret_cast<const char *>(d.data()), d.size());
    mpack_tree_parse(&tree);
    mpack_node_t root = mpack_tree_root(&tree);

    // get the values out of it
    const auto handle = mpack_node_u64(mpack_node_map_cstr(root, "port"));
    const auto id = mpack_node_u64(mpack_node_map_cstr(root, "id"));

    // clean up
    auto status = mpack_tree_destroy(&tree);
    if(status != mpack_ok) {
        fprintf(stderr, "%s failed: %d\n", "mpack_tree_destroy", status);
    }

    return {handle, id};
}

/**
 * Initializes the disk user client.
 */
Disk::Disk(const std::shared_ptr<IoStream> &io, const std::string_view &_forestPath,
        const uint64_t diskId) : DiskDriverClient(io), forestPath(_forestPath), id(diskId) {
    int err;

    // try to open a disk access session
    const auto ret = this->OpenSession();
    if(ret.status) {
        this->status = ret.status;
        return;
    }

    this->sessionToken = ret.sessionToken;
    this->commandVmRegion = ret.regionHandle;

    const auto pageSz = sysconf(_SC_PAGESIZE);
    const auto size = ((ret.regionSize + pageSz - 1) / pageSz) * pageSz;

    // map the region
    uintptr_t base{0};
    err = MapVirtualRegionRange(this->commandVmRegion, kMappingRange, size, 0, &base);
    kMappingRange[0] += size; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        this->status = err;
        return;
    }

    this->commandList = reinterpret_cast<volatile Command *>(base);
}

/**
 * Cleans up all resources related to the disk, including shared memory buffers.
 */
Disk::~Disk() {
    int err;

    // unmap VM objects

    // notify other side we're going away
    if(this->sessionToken) {
        err = this->CloseSession(this->sessionToken);
        if(err) {
            fprintf(stderr, "[%s] failed to close: %d\n", "disk", err);
        }
    }
}



/**
 * Gets the capacity of the disk.
 */
int Disk::GetCapacity(std::pair<uint32_t, uint64_t> &outCapacity) {
    const auto ret = this->GetCapacity(this->id);
    if(!ret.status) outCapacity = {ret.sectorSize, ret.numSectors};
    return ret.status;
}

/**
 * Performs a slow read which copies data directly via the message.
 *
 * @return 0 on success, negative error code otherwise
 */
int Disk::Read(const uint64_t sector, const size_t numSectors, std::vector<std::byte> &out) {
    return -1;
}

