#include "AtaDiskRpcServer.h"
#include "AtaDisk.h"

#include "Log.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include <unistd.h>
#include <sys/syscalls.h>
#include <driver/BufferPool.h>
#include <rpc/rt/ServerPortRpcStream.h>
#include <DriverSupport/disk/Types.h>

/// Region of virtual memory space for command buffers
static uintptr_t kCommandRegionMappingRange[2] = {
    // start
    0x67808000000,
    // end
    0x67809000000,
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

    if(kLogSessionLifecycle) Trace("Open session $%lx", id);

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
    if(kLogSessionLifecycle) Trace("Close session $%lx", token);

    // get the session
    std::lock_guard<std::mutex> lg(this->sessionsLock);
    if(!this->sessions.contains(token)) return Errors::InvalidSession;
    auto &session = this->sessions[token];

    // TODO: check for outstanding commands
    // unmap command region
    UnmapVirtualRegion(session.commandVmRegion);

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
AtaDiskRpcServer::CreateReadBufferReturn AtaDiskRpcServer::implCreateReadBuffer(uint64_t token,
        uint64_t requested) {
    int err;

    // get the session
    std::lock_guard<std::mutex> lg(this->sessionsLock);
    if(!this->sessions.contains(token)) return {Errors::InvalidSession};
    auto &session = this->sessions[token];

    // return the info for the existing read buffer if we have one already
    if(session.readBuf) {
        const auto &buf = session.readBuf;
        return {0, buf->getHandle(), buf->getMaxSize()};
    }

    // figure out initial allocation and create the buffer
    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(!pageSz) return {Errors::InternalError};

    size_t initialSize = std::min(std::max(requested, kReadBufferMinSize), kReadBufferMaxSize);
    initialSize = ((initialSize + pageSz - 1) / pageSz) * pageSz;
    if(kLogBufferRequests) Trace("Create read buffer for $%lx: requested %lu bytes, got %lu",
            token, requested, initialSize);

    err = libdriver::BufferPool::Alloc(initialSize, kReadBufferMaxSize, session.readBuf);
    if(err) {
        return {err};
    }

    // return buffer information
    const auto &buf = session.readBuf;
    if(!buf) {
        return {Errors::InternalError};
    }

    return {0, buf->getHandle(), buf->getMaxSize()};
}

/**
 * Allocates the write buffer region. This is implemented in the same way as the read buffer and
 * the same alignment caveats apply.
 */
AtaDiskRpcServer::CreateWriteBufferReturn AtaDiskRpcServer::implCreateWriteBuffer(uint64_t session,
        uint64_t requested) {
    if(kLogBufferRequests) Trace("Create write buffer for $%lx: requested %lu bytes", session,
            requested);
    return {Errors::Unsupported};
}

/**
 * Indicates that the given command slot has been fully built up in the shared memory region by the
 * caller, and we should queue it to the associated device.
 */
void AtaDiskRpcServer::implExecuteCommand(uint64_t token, uint32_t slot) {
    // get the session
    this->sessionsLock.lock();
    if(!this->sessions.contains(token)) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "invalid session token", slot);
        return this->sessionsLock.unlock();
    }
    auto &session = this->sessions[token];
    this->sessionsLock.unlock();

    // check its command buffer and validate the command
    if(slot >= session.numCommands) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "invalid command slot", slot);
        return;
    }
    auto &command = session.commandList[slot];

    if(!command.allocated || command.completed) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "invalid command state", slot);
        return;
    }
    if(__atomic_test_and_set(&command.busy, __ATOMIC_ACQUIRE)) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "command already busy", slot);
        return;
    }

    // begin processing it
    this->processCommand(session, slot, command);
}

/**
 * Indicates the caller is done accessing the read buffer region associated with the given read
 * buffer, and the command slot (and its associated read buffer allocation) may be reused for
 * another operation.
 */
void AtaDiskRpcServer::implReleaseReadCommand(uint64_t token, uint32_t slot) {
    // get the session
    this->sessionsLock.lock();
    if(!this->sessions.contains(token)) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "invalid session token", slot);
        return this->sessionsLock.unlock();
    }
    auto &session = this->sessions[token];
    this->sessionsLock.unlock();

    // validate slot index and get reference to command
    if(slot >= session.numCommands) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "invalid command slot", slot);
        return;
    }
    auto &command = session.commandList[slot];

    if(command.type != DriverSupport::disk::CommandType::Read) {
        Warn("%s: Session $%lx %s (slot %lu)", __FUNCTION__, token, "invalid type", slot);
        return;
    }

    // release the buffer and the command slot
    session.readCommandBuffers.erase(slot);

    command.notifyThread = 0;
    command.notifyBits = 0;
    command.diskId = 0;
    command.sector = 0;
    command.bufferOffset = 0;
    command.numSectors = 0;
    command.bytesTransfered = 0;

    __atomic_clear(&command.busy, __ATOMIC_RELAXED);
    __atomic_clear(&command.completed, __ATOMIC_RELAXED);
    __atomic_clear(&command.allocated, __ATOMIC_RELEASE);
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



/**
 * Attempts to process the given command.
 */
void AtaDiskRpcServer::processCommand(Session &session, const size_t slot,
        volatile DriverSupport::disk::Command &cmd) {
    using namespace DriverSupport::disk;

    switch(cmd.type) {
        // start a read request
        case CommandType::Read:
            if(!cmd.numSectors || !cmd.notifyThread || !cmd.notifyBits) {
                Warn("%s: Invalid %s command in slot %lu (%lu sectors, notify %p:%p)",
                        __FUNCTION__, "read", slot, cmd.numSectors, cmd.notifyThread, cmd.notifyBits);
                return;
            }

            // we can do the read now
            this->doCmdRead(session, slot, cmd);
            break;

        // other types currently unsupported
        default:
            Warn("%s: Unsupported command type $%02x in slot %lu", __FUNCTION__, cmd.type, slot);
            break;
    }
}

/**
 * Processes a read command. We assume that the contents of the command have been validated when we
 * are called.
 */
void AtaDiskRpcServer::doCmdRead(Session &session, const size_t slot,
        volatile DriverSupport::disk::Command &cmd) {
    int err;

    // get the disk
    std::shared_ptr<AtaDisk> disk;
    {
        const uint64_t diskId = cmd.diskId;
        std::lock_guard<std::mutex> lg(this->disksLock);
        if(this->disks.contains(diskId)) {
            disk = this->disks[diskId].lock();
        }
    }
    if(!disk) {
        Warn("%s: Invalid disk id ($%lx) in %s command at %lu", __FUNCTION__, cmd.diskId, "read",
                slot);
        return;
    }

    // allocate the buffer
    const size_t readBytes = disk->getSectorSize() * cmd.numSectors;
    if(kLogIoRequests) Trace("Read request is %lu bytes (sector $%lx)", readBytes, cmd.sector);

    std::shared_ptr<libdriver::BufferPool::Buffer> buffer;
    err = session.readBuf->getBuffer(readBytes, buffer);

    if(err) {
        Warn("%s: Failed to get %s buffer (%lu bytes)", __FUNCTION__, "read", readBytes);
        return this->notifyCmdFailure(cmd, err);
    }

    session.readCommandBuffers[slot] = buffer;
    cmd.bufferOffset = buffer->getPoolOffset();

    // perform the read
    err = disk->read(cmd.sector, cmd.numSectors, buffer, [this, readBytes, &cmd](bool success) {
        if(success) {
            cmd.bytesTransfered = readBytes;

            this->notifyCmdSuccess(cmd);
        } else {
            this->notifyCmdFailure(cmd, Errors::IoError);
        }
    });

    if(err) {
        Warn("%s: Failed to submit %s request (start %lu x %lu sectors)", __FUNCTION__, "read",
                cmd.sector, cmd.numSectors);
        return this->notifyCmdFailure(cmd, err);
    }
}



/**
 * Updates the command object with the given status code and and signals the remote thread.
 */
void AtaDiskRpcServer::notifyCmdCompletion(volatile DriverSupport::disk::Command &cmd,
        const int status) {
    int err;

    // update command object
    __atomic_store_n(&cmd.status, status, __ATOMIC_RELAXED);
    __atomic_store_n(&cmd.completed, true, __ATOMIC_RELAXED);
    __atomic_clear(&cmd.busy, __ATOMIC_RELEASE);

    // notify the thread
    err = NotificationSend(cmd.notifyThread, cmd.notifyBits);
    if(err) {
        Warn("%s: %s failed: %d", __FUNCTION__, "NotificationSend", err);
    }
}

