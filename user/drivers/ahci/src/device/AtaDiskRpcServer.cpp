#include "AtaDiskRpcServer.h"
#include "AtaDisk.h"

#include "Log.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include <unistd.h>
#include <sys/syscalls.h>
#include <rpc/rt/ServerPortRpcStream.h>
#include <DriverSupport/disk/Types.h>

/// Region of virtual memory space for command buffers
static uintptr_t kCommandRegionMappingRange[2] = {
    // start
    0x67808000000,
    // end
    0x67810000000,
};
AtaDiskRpcServer *AtaDiskRpcServer::gShared{nullptr};

/**
 * Allocates the shared instance if needed.
 */
AtaDiskRpcServer *AtaDiskRpcServer::the() {
    // XXX: this can be racey
    if(!gShared) {
        // create listening port and IO stream
        uintptr_t handle;
        int err = PortCreate(&handle);
        REQUIRE(!err, "%s failed: %d", "PortCreate", err);

        auto strm = std::make_shared<rpc::rt::ServerPortRpcStream>(handle);
        gShared = new AtaDiskRpcServer(strm, handle);
    }

    return gShared;
}

/**
 * Initializes the RPC server. We'll allocate a listening port as needed for it and then start the
 * processing thread.
 */
AtaDiskRpcServer::AtaDiskRpcServer(const std::shared_ptr<IoStream> &s, const uintptr_t port) :
    DiskDriverServer(s), listenPort(port) {
    // create the worker thread
    this->worker = std::make_unique<std::thread>(&AtaDiskRpcServer::main, this);
}

/**
 * Cleans up the listening port and thread.
 */
AtaDiskRpcServer::~AtaDiskRpcServer() {
    PortDestroy(this->listenPort);

    this->worker->join();
}

/**
 * Main loop for the worker thread
 */
void AtaDiskRpcServer::main() {
    ThreadSetName(0, "ATA Disk RPC server");

    while(this->workerRun) {
        this->runOne(true);
    }
}

/**
 * Registers a disk with the RPC handler.
 */
uint64_t AtaDiskRpcServer::add(const std::shared_ptr<AtaDisk> &disk) {
    std::lock_guard<std::mutex> lg(this->disksLock);

    const auto id = this->nextId++;
    this->disks.emplace(id, disk);

    return id;
}

/**
 * Removes a disk based on its id.
 */
bool AtaDiskRpcServer::remove(const uint64_t id) {
    std::lock_guard<std::mutex> lg(this->disksLock);
    return !!this->disks.erase(id);
}


/**
 * Sizes the given disk.
 */
AtaDiskRpcServer::GetCapacityReturn AtaDiskRpcServer::implGetCapacity(uint64_t diskId) {
    std::shared_ptr<AtaDisk> disk;

    // get the disk
    {
        std::lock_guard<std::mutex> lg(this->disksLock);
        if(this->disks.contains(diskId)) {
            disk = this->disks[diskId].lock();
        }
    }
    if(!disk) {
        return {Errors::NoSuchDisk};
    }

    // return sizing information
    return {0, disk->getSectorSize(), disk->getNumSectors()};
}



/**
 * Opens a new session for IO against the disks controlled by this connection. We'll allocate some
 * shared memory to hold the command descriptors.
 */
AtaDiskRpcServer::OpenSessionReturn AtaDiskRpcServer::implOpenSession() {
    int err;

    // Allocate a session
    Session s;

    const auto pageSz = sysconf(_SC_PAGESIZE);
    size_t commandRegionSize = s.numCommands * sizeof(DriverSupport::disk::Command);
    commandRegionSize = ((commandRegionSize + pageSz - 1) / pageSz) * pageSz;

    err = AllocVirtualAnonRegion(commandRegionSize, (VM_REGION_RW | VM_REGION_FORCE_ALLOC),
            &s.commandVmRegion);
    if(err) {
        return {err};
    }

    // map its command area
    uintptr_t base{0};
    err = MapVirtualRegionRange(s.commandVmRegion, kCommandRegionMappingRange, commandRegionSize,
            0, &base);
    kCommandRegionMappingRange[0] += commandRegionSize; // XXX: this seems like it shouldn't be necessary...
    if(err) {
        return {err};
    }

    s.commandList = reinterpret_cast<volatile DriverSupport::disk::Command *>(base);

    // store it
    uint64_t id;
    {
        std::lock_guard<std::mutex> lg(this->sessionsLock);
        id = this->nextSessionId++;
        this->sessions.emplace(id, s);
    }

    // return info on it
    return {0, id, s.commandVmRegion, commandRegionSize, s.numCommands};
}

/**
 * Closes a previously allocated session. If the session has any commands that are in flight, we
 * will try to abort them (if they haven't been issued to hardware yet) and if not possible, we
 * mark them as unavailable so the completion isn't invoked, and delay the deallocation of the
 * shared memory regions until all commands are complete.
 *
 * Note that this call will return immediately, since the client can unmap the regions immediately
 * without problems.
 */
int32_t AtaDiskRpcServer::implCloseSession(uint64_t token) {
    Trace("Close session $%lx", token);

    // get the session
    std::lock_guard<std::mutex> lg(this->sessionsLock);
    if(!this->sessions.contains(token)) return Errors::InvalidSession;
    auto &session = this->sessions[token];

    // TODO: check for outstanding commands
    // TODO: unmap regions
    Trace("Need to unmap region $%p'h", session.commandVmRegion);

    // remove session
    this->sessions.erase(token);
    return 0;
}

/**
 * Allocates the read buffer region. All read requests will place their memory inside this buffer,
 * which is a libdriver scatter/gather alloc pool. This is one contiguous virtual memory allocation
 * which can back multiple scatter gather buffers.
 *
 * We align the buffers inside this region to 512 byte boundaries, which are a common sector size
 * for most drives or a multiple thereof.
 *
 * The caller should map this region as read-only.
 */
AtaDiskRpcServer::CreateReadBufferReturn AtaDiskRpcServer::implCreateReadBuffer(uint64_t session,
        uint64_t requested) {
    Trace("Create read buffer for $%lx: requested %lu bytes", session, requested);
    return {Errors::Unsupported};
}

/**
 * Allocates the write buffer region. This is implemented in the same way as the read buffer and
 * the same alignment caveats apply.
 */
AtaDiskRpcServer::CreateWriteBufferReturn AtaDiskRpcServer::implCreateWriteBuffer(uint64_t session,
        uint64_t requested) {
    Trace("Create write buffer for $%lx: requested %lu bytes", session, requested);
    return {Errors::Unsupported};
}

/**
 * Indicates that the given command slot has been fully built up in the shared memory region by the
 * caller, and we should queue it to the associated device.
 */
void AtaDiskRpcServer::implExecuteCommand(uint64_t session, uint32_t slot) {
    Trace("Session $%lx: Execute command %lu", session, slot);
}

/**
 * Indicates the caller is done accessing the read buffer region associated with the given read
 * buffer, and the command slot (and its associated read buffer allocation) may be reused for
 * another operation.
 */
void AtaDiskRpcServer::implReleaseReadCommand(uint64_t session, uint32_t slot) {
    Trace("Session $%lx: Release read command %lu", session, slot);
}

/**
 * Attempts to allocate a region of the given size in the write buffer, which may then be used as
 * part of a write command.
 */
AtaDiskRpcServer::AllocWriteMemoryReturn AtaDiskRpcServer::implAllocWriteMemory(uint64_t session,
        uint64_t bytesRequested) {
    Trace("Session $%lx: Allocate write %lu bytes write buffer", session, bytesRequested);
    return {Errors::Unsupported};
}
