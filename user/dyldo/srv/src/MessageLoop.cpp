#include "MessageLoop.h"
#include "Library.h"
#include "hashmap.h"
#include "Log.h"
#include "PacketTypes.h"

#include <unistd.h>
#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include <sys/elf.h>
#include <sys/syscalls.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

/// Region of virtual memory space for temporary mappings
static uintptr_t kTempMappingRange[2] = {
    // start
    0x10000000000,
    // end (256G later)
    0x14000000000,
};

/**
 * Initializes the message loop, including its receive buffer. This will also create the port and
 * register it.
 */
MessageLoop::MessageLoop() {
    int err;

    // set up libraries map
    this->libraries = hashmap_new(sizeof(HashmapEntry), 16, 'DYLD', 'LIBS', HashLib, CompareLib,
            nullptr);

    // allocate receive and send buffers
    err = posix_memalign(&this->rxBuf, 16, kMaxMsgLen);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
    }

    err = posix_memalign(&this->txBuf, 16, kMaxMsgLen);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
    }

    // allocate port and register it
    err = PortCreate(&this->port);
    if(err) {
        Abort("%s failed: %d", "PortCreate", err);
    }

    err = RegisterService(kPortName.data(), this->port);
    if(err) {
        Abort("%s failed: %d", "RegisterService", err);
    }
}

/**
 * Tears down message loop resources.
 */
MessageLoop::~MessageLoop() {
    hashmap_free(this->libraries);

    PortDestroy(this->port);
    free(this->rxBuf);
    free(this->txBuf);
}


/**
 * Calculates the hash of the given hashmap entry. This is computed by hashing the string.
 */
uint64_t MessageLoop::HashLib(const void *item, uint64_t s0, uint64_t s1) {
    auto e = reinterpret_cast<const HashmapEntry *>(item);
    return hashmap_sip(e->path, e->pathLen, s0, s1);
}

/**
 * Compares two libraries by performing a string compare.
 */
int MessageLoop::CompareLib(const void *a, const void *b, void *) {
    auto e1 = reinterpret_cast<const HashmapEntry *>(a);
    auto e2 = reinterpret_cast<const HashmapEntry *>(b);

    const auto len = std::min(e1->pathLen, e2->pathLen);
    return strncmp(e1->path, e2->path, len);
}



/**
 * Main loop; we'll wait to receive messages forever from the port.
 */
void MessageLoop::run() {
    int err;

    // set up receive buffer
    auto msg = reinterpret_cast<struct MessageHeader *>(this->rxBuf);
    memset(msg, 0, sizeof(*msg));

    // wait to receive messages
    Success("Waiting to receive messages on port $%p'h", this->port);

    while(true) {
        err = PortReceive(this->port, msg, kMaxMsgLen, UINTPTR_MAX);

        if(err < 0) {
            Abort("%s failed: %d", "PortReceive", err);
        } else if(msg->receivedBytes < sizeof(rpc::RpcPacket)) {
            Warn("Received too small RPC message (%lu bytes)", msg->receivedBytes);
            continue;
        }

        // handle the packet
        auto packet = reinterpret_cast<rpc::RpcPacket *>(msg->data);

        switch(packet->type) {
            case static_cast<uint32_t>(DyldosrvMessageType::MapSegment):
                this->handleMapSegment(*msg);
                break;

            default:
                Warn("Unknown RPC message type: $%08x", packet->type);
                break;
        }
    }
}

/**
 * Handles a request to map a segment of a dynamic library.
 */
void MessageLoop::handleMapSegment(const struct MessageHeader &msg) {
    int err;
    bool didAdd{false};
    void *temp{nullptr};
    Library *library{nullptr};
    uintptr_t segRegion{0};

    // validate it
    auto packet = reinterpret_cast<const rpc::RpcPacket *>(msg.data);
    const size_t payloadBytes = msg.receivedBytes - sizeof(rpc::RpcPacket);
    if(payloadBytes < sizeof(DyldosrvMapSegmentRequest)) {
        Warn("Received too small RPC message (%lu bytes, type $%08x)", msg.receivedBytes,
                packet->type);
        return;
    }

    auto req = reinterpret_cast<const DyldosrvMapSegmentRequest *>(packet->payload);
    const size_t nameBytes = payloadBytes - sizeof(DyldosrvMapSegmentRequest);
    if(nameBytes < 2) {
        Warn("Name length too short!");
        return;
    }

    // get matching library object
    HashmapEntry ent{
        // XXX: we should probably use strnlen()
        req->path, strlen(req->path), nullptr
    };
    temp = hashmap_get(this->libraries, &ent);
    if(!temp) goto notfound;

    library = reinterpret_cast<HashmapEntry *>(temp)->library;

    // look up the segment
    segRegion = library->regionFor(req->phdr);
    if(!segRegion) goto notfound;

    // map it in the requesting task and reply
    err = this->mapSegment(msg, *req, segRegion);

    if(err) {
        return this->reply(msg, *req, err);
    } else {
        return this->reply(msg, *req, segRegion);
    }

notfound:;
    // added the segment but then failed to find it? something is fucked
    if(didAdd) {
        return this->reply(msg, *req, DyldosrvErrors::InternalError);
    }

    // load segment from file
    err = this->loadSegment(msg, *req, segRegion);
    if(err) {
        Warn("Failed to load segment from %s: %d", req->path, err);
        return this->reply(msg, *req, err);
    }

    didAdd = true;
    this->reply(msg, *req, segRegion);
}

/**
 * Loads the segment described by the RPC message from the dynamic library and stores information
 * about it.
 *
 * @return 0 if the segment was loaded (and stored in the map) error code otherwise
 */
int MessageLoop::loadSegment(const struct MessageHeader &msg, const DyldosrvMapSegmentRequest &req,
        uintptr_t &outRegion) {
    int err, ret{-1};
    uintptr_t region{0}, base, flags{VM_REGION_READ};
    const auto pageSz = sysconf(_SC_PAGESIZE);

    // try opening the file
    FILE *f = fopen(req.path, "rb");
    if(!f) {
        Warn("Failed to open '%s': %d", req.path, errno);
        return -errno;
    }

    // TODO: verify provided phdr against file
    const auto &phdr = req.phdr;

    // allocate the VM region (RW initially) and map it in our address space
    const auto vmBase = phdr.p_vaddr + req.objectVmBase;
    const auto vmStart = (vmBase / pageSz) * pageSz;
    const auto vmEnd = ((((vmBase + phdr.p_memsz) + pageSz - 1) / pageSz) * pageSz) - 1;
    const size_t vmBytes = (vmEnd - vmStart) + 1;

    err = AllocVirtualAnonRegion(vmBytes, VM_REGION_RW, &region);
    if(err) {
        Warn("%s failed: %d", "AllocVirtualAnonRegion", err);
        goto fail;
    }

    err = MapVirtualRegionRange(region, kTempMappingRange, vmBytes, 0, &base);
    if(err) {
        Warn("%s failed: %d", "MapVirtualRegionRange", err);
        goto fail;
    }

    kTempMappingRange[0] += vmBytes; // XXX: ugh...

    // read file contents
    if(phdr.p_filesz) {
        auto copyTo = reinterpret_cast<void *>(base + (phdr.p_offset % pageSz));

        err = fseek(f, phdr.p_offset, SEEK_SET);
        if(err) {
            Warn("%s failed: %d", "fseek", errno);
            goto fail;
        }

        err = fread(copyTo, 1, phdr.p_filesz, f);
        if(err != phdr.p_filesz) {
            Warn("Failed to read $%x bytes from $%x: %d", phdr.p_filesz, phdr.p_offset, err);
            goto fail;
        }
    }

    // update protection
    if(phdr.p_flags & PF_X) {
        flags |= VM_REGION_EXEC;
    }
    if(phdr.p_flags & PF_W) {
        flags |= VM_REGION_WRITE;
    }

    if((flags & VM_REGION_EXEC) && (flags & VM_REGION_WRITE)) {
        Warn("Refusing to add W+X mapping at %p (library %s) in task $%p'h", vmStart, req.path,
                msg.senderTask);
        goto fail;
    }

    err = VirtualRegionSetFlags(region, flags);
    if(err) {
        Warn("%s failed: %d", "VirtualRegionSetFlags", err);
        goto fail;
    }
 
    // then map it in the requesting task's address space
    err = MapVirtualRegionRemote(msg.senderTask, region, vmStart, vmBytes, 0);
    if(err) {
        Warn("%s failed: %d", "MapVirtualRegionRemote", err);
        goto fail;
    }

    // and unmap from our address space
    err = UnmapVirtualRegion(region);
    if(err) {
        Warn("%s failed: %d", "UnmapVirtualRegion", err);
    }

    // store information about it
    this->storeInfo(req, region);
    outRegion = region;

    ret = 0;
    goto beach;

    // error handling
fail:;
     if(region) {
         DeallocVirtualRegion(region);
     }

    // clean up
beach:;
    fclose(f);
    return ret;
}

/**
 * Extracts the needed path from the request, and uses this plus the newly allocated virtual memory
 * region to store information.
 */
void MessageLoop::storeInfo(const DyldosrvMapSegmentRequest &req, const uintptr_t vmRegion) {
    HashmapEntry ent{
        // XXX: we should probably use strnlen()
        req.path, strlen(req.path), nullptr
    };

    // have we got a library already?
    auto item = hashmap_get(this->libraries, &ent);
    if(item) {
        auto libe = reinterpret_cast<HashmapEntry *>(item);

        libe->library->addSegment(req, vmRegion);
        return;
    }

    // no; create one and insert it
    auto copyPath = strndup(req.path, ent.pathLen);
    if(!copyPath) {
        Warn("%s failed: %d", "strdup", errno);
        return;
    }

    auto library = new Library(copyPath);
    library->addSegment(req, vmRegion);

    ent.path = copyPath;
    ent.library = library;

    auto ret = hashmap_set(this->libraries, &ent);
    if(!ret && hashmap_oom(this->libraries)) {
        Abort("%s failed: %d", "hashmap_set", ENOMEM);
    }
}

/**
 * Maps a segment we've already loaded previously in the calling task.
 *
 * @param msg Message struct containing the sender task/thread
 * @param req Load request from the client
 * @param region Handle to the VM object containing the shareable code/data
 */
int MessageLoop::mapSegment(const struct MessageHeader &msg, const DyldosrvMapSegmentRequest &req,
        const uintptr_t region) {
    int err;
    const auto pageSz = sysconf(_SC_PAGESIZE);

    // TODO: verify phdr against file
    const auto &phdr = req.phdr;

    // allocate the VM region (RW initially) and map it in our address space
    const auto vmBase = phdr.p_vaddr + req.objectVmBase;
    const auto vmStart = (vmBase / pageSz) * pageSz;
    const auto vmEnd = ((((vmBase + phdr.p_memsz) + pageSz - 1) / pageSz) * pageSz) - 1;
    const size_t vmBytes = (vmEnd - vmStart) + 1;

    // perform mapping
    err = MapVirtualRegionRemote(msg.senderTask, region, vmStart, vmBytes, 0);
    if(err) {
        Warn("%s failed: %d", "MapVirtualRegionRemote", err);
        return err;
    }

    return 0;
}

/**
 * Sends a success reply to a previously received map request.
 */
void MessageLoop::reply(const struct MessageHeader &msg, const DyldosrvMapSegmentRequest &req,
        const uintptr_t vmRegion) {
    int err;
    auto inPacket = reinterpret_cast<const rpc::RpcPacket *>(msg.data);

    // construct the reply message
    const size_t msgBytes = sizeof(rpc::RpcPacket) + sizeof(DyldosrvMapSegmentReply);
    memset(this->txBuf, 0, msgBytes);

    auto outPacket = reinterpret_cast<rpc::RpcPacket *>(this->txBuf);
    auto outMsg = reinterpret_cast<DyldosrvMapSegmentReply *>(outPacket->payload);

    outPacket->type = static_cast<uint32_t>(DyldosrvMessageType::MapSegmentReply);
    outMsg->status = 0;
    outMsg->vmRegion = vmRegion;

    // send it
    err = PortSend(inPacket->replyPort, outPacket, msgBytes);
    if(err) {
        Warn("%s failed: %d", "PortSend", err);
    }
}

/**
 * Sends an error reply to a previously received map request.
 */
void MessageLoop::reply(const struct MessageHeader &msg, const DyldosrvMapSegmentRequest &req,
        const int outErr) {
    int err;
    auto inPacket = reinterpret_cast<const rpc::RpcPacket *>(msg.data);

    // construct the reply message
    const size_t msgBytes = sizeof(rpc::RpcPacket) + sizeof(DyldosrvMapSegmentReply);
    memset(this->txBuf, 0, msgBytes);

    auto outPacket = reinterpret_cast<rpc::RpcPacket *>(this->txBuf);
    auto outMsg = reinterpret_cast<DyldosrvMapSegmentReply *>(outPacket->payload);

    outPacket->type = static_cast<uint32_t>(DyldosrvMessageType::MapSegmentReply);
    outMsg->status = outErr;

    // send it
    err = PortSend(inPacket->replyPort, outPacket, msgBytes);
    if(err) {
        Warn("%s failed: %d", "PortSend", err);
    }
}

