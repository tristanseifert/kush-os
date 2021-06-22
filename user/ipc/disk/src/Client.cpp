#include "Client.h"

#include <atomic>
#include <cstdio>

#include <unistd.h>
#include <mpack/mpack.h>
#include <driver/DrivermanClient.h>
#include <rpc/rt/ClientPortRpcStream.h>

using namespace DriverSupport::disk;

/// Region of virtual memory space for command buffers
static uintptr_t kCommandMappingRange[2] = {
    // start
    0x67800000000,
    // end
    0x67801000000,
};
/// Region of virtual memory space for disk read/write buffers
static uintptr_t kIoBufferMappingRange[2] = {
    // start
    0x67810000000,
    // end
    0x67820000000,
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
    this->numCommands = ret.numCommands;

    const auto pageSz = sysconf(_SC_PAGESIZE);
    const auto size = ((ret.regionSize + pageSz - 1) / pageSz) * pageSz;

    // map the region
    uintptr_t base{0};
    err = MapVirtualRegionRange(this->commandVmRegion, kCommandMappingRange, size, 0, &base);
    kCommandMappingRange[0] += size; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        this->status = err;
        return;
    }

    this->commandList = reinterpret_cast<volatile Command *>(base);

    // get the size information
    auto ret2 = this->GetCapacity(this->id);
    if(ret2.status) {
        this->status = ret2.status;
        return;
    }

    this->sectorSize = ret2.sectorSize;
    this->numSectors = ret2.numSectors;
}

/**
 * Cleans up all resources related to the disk, including shared memory buffers.
 */
Disk::~Disk() {
    int err;

    // unmap VM objects
    if(this->readBufVmRegion) {
        UnmapVirtualRegion(this->readBufVmRegion);
    }

    // notify other side we're going away
    if(this->sessionToken) {
        err = this->CloseSession(this->sessionToken);
        if(err) {
            fprintf(stderr, "[%s] failed to close: %d\n", "disk", err);
        }

        UnmapVirtualRegion(this->commandVmRegion);
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
    int err;

    // ensure the read buffer region is allocated
    err = this->ensureReadBuffer();
    if(err) return err;

    // get caller information
    uintptr_t thisThread, notify;
    err = ThreadGetHandle(&thisThread);
    if(err) return err;

    // get a command slot and build up the read request
    const auto slotIdx = this->allocCommandSlot();
    if(slotIdx == -1) return Errors::NoCommandsAvailable;
    auto &command = this->commandList[slotIdx];

    command.type = CommandType::Read;
    command.status = 0;

    command.notifyThread = thisThread;
    command.notifyBits = kCommandCompletionBits;
    command.diskId = this->id;
    command.sector = sector;
    command.bufferOffset = 0;
    command.numSectors = numSectors;
    command.bytesTransfered = 0;

    // submit command and then await completion
    this->submit(slotIdx);

    do {
        notify = NotificationReceive(kCommandCompletionBits, UINTPTR_MAX);
    } while(!__atomic_load_n(&command.completed, __ATOMIC_RELAXED));

    // copy out from buffer if command was successful
    const int status = command.status;
    if(!status) {
        // copy the data
        auto ptr = reinterpret_cast<std::byte *>(reinterpret_cast<uintptr_t>(this->readBuf) +
                command.bufferOffset);
        const size_t numBytes = command.bytesTransfered;

        out.reserve(numBytes);
        out.insert(out.end(), ptr, ptr+numBytes);
    } else {
        fprintf(stderr, "[%s] Command failed! %d\n", "disk", status);
    }

    // release the command object and return its status
    this->ReleaseReadCommand(this->sessionToken, slotIdx);
    return status;
}



/**
 * Attempts to allocate a command slot that a command can be used to prepare a new command.
 *
 * @return Command slot index of the allocated slot, or -1 if no slots available.
 */
size_t Disk::allocCommandSlot() {
    for(size_t i = 0; i < this->numCommands; i++) {
        auto &command = this->commandList[i];
        if(!__atomic_test_and_set(&command.allocated, __ATOMIC_RELAXED)) {
            return i;
        }
    }

    return -1;
}

/**
 * Submits the command in the given command slot.
 */
void Disk::submit(const size_t slot) {
    // ensure any writes to shared memory post
    std::atomic_thread_fence(std::memory_order_release);

    // then submit the request
    this->ExecuteCommand(this->sessionToken, slot);
}

/**
 * Ensures we have a read buffer allocated. If this is the first time this is called, we'll make
 * the RPC request to ensure there's a read buffer region.
 *
 * TODO: Add locking to ensure this can only be called from one thread at a time
 */
int Disk::ensureReadBuffer() {
    int err;

    // bail if we've already got one
    if(this->readBuf) return 0;

    // make the setup request. we don't request a particular size
    auto ret = this->CreateReadBuffer(this->sessionToken, 0);
    if(ret.status) return ret.status;

    // map the buffer
    uintptr_t base{0};
    err = MapVirtualRegionRange(ret.readBufHandle, kIoBufferMappingRange, ret.readBufMaxSize,
            VM_REGION_READ, &base);
    kCommandMappingRange[0] += ret.readBufMaxSize; // XXX: yike fix this
    if(err) return err;

    this->readBufVmRegion = ret.readBufHandle;
    this->readBufMaxSize = ret.readBufMaxSize;
    this->readBuf = reinterpret_cast<void *>(base);

    return 0;
}
