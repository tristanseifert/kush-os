#include "FileIo.h"

#include "rpc/dispensary.h"
#include "rpc/file.h"
#include "sys/infopage.h"
#include "sys/syscalls.h"

#include "helpers/Send.h"

#include <malloc.h>

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <cista/serialization.h>
#include <rpc/RpcPacket.hpp>
#include <rpc/FileIO.hpp>

using namespace fileio;
using namespace rpc;

/**
 * Performs file reads if the direct IO strategy is available.
 *
 * This reads the file in small chunks via direct message passing.
 */
static int FileReadDirect(const uintptr_t file, const uint64_t _offset, const size_t length,
        void *outBuf) {
    int err;
    void *rxBuf = nullptr;

    // calculate how many IO requests we need to make
    size_t numChunks = 1, chunkSize = length;

    if(gState.maxIoSize && length > gState.maxIoSize) {
        numChunks = (length + gState.maxIoSize - 1) / gState.maxIoSize;
        chunkSize = gState.maxIoSize;
    }

    // set up a buffer for the send request (128 = fixed overhead for rest of read req fields)
    auto rxBufSize = chunkSize + (128) + sizeof(RpcPacket) + sizeof(MessageHeader);
    rxBufSize = ((rxBufSize + 15) / 16) * 16;

    err = posix_memalign(&rxBuf, 16, rxBufSize);
    if(err) {
        return err;
    }

    // perform the required number of reads
    uint64_t offset = _offset;
    size_t bytesRead = 0;

    for(size_t i = 0; i < numChunks; i++) {
        // send the request
        FileIoReadReq req;
        req.file = file;
        req.offset = offset;
        req.length = std::min(chunkSize, (length - bytesRead));

        auto requestBuf = cista::serialize(req);

        err = _RpcSend(gState.ioServerPort, static_cast<uint32_t>(FileIoEpType::ReadFileDirect),
                requestBuf, gState.replyPort);
        if(err) goto fail;

        // receive response
        struct MessageHeader *msg = (struct MessageHeader *) rxBuf;
        err = PortReceive(gState.replyPort, msg, rxBufSize, UINTPTR_MAX);

        if(err > 0) {
            // read out the type
            if(msg->receivedBytes < sizeof(RpcPacket)) {
                err = -50;
                goto fail;
            }

            const auto packet = reinterpret_cast<RpcPacket *>(msg->data);
            if(packet->type != static_cast<uint32_t>(FileIoEpType::ReadFileDirectReply)) {
                fprintf(stderr, "%s received wrong packet type %08x!\n", __PRETTY_FUNCTION__, packet->type);
                err = -50;
                goto fail;
            }

            // deserialize the capabilities response
            auto data = std::span(packet->payload, err - sizeof(RpcPacket));

            auto req = cista::deserialize<FileIoReadReqReply>(data);

            if(req->status < 0) {
                err = req->status;
                goto fail;
            }

            // the read succeeded at least partially
            if(req->data.empty()) {
                goto beach;
            }

            const auto toCopy = std::min(req->data.size(), (length - bytesRead));
            void *writePtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(outBuf) + bytesRead);
            memcpy(writePtr, req->data.data(), toCopy);

            // bookkeeping
            bytesRead += toCopy;
            offset += toCopy;
        }
        // read error
        else {
            goto fail;
        }
    }

beach:;
    // finished reading; sum up the number of bytes read
    err = bytesRead;

fail:;
    // clean up
    free(rxBuf);
    return err;
}

/**
 * Wrapper around the file read.
 *
 * We split the IO into chunks that are a multiple of the server IO block size.
 */
int FileRead(const uintptr_t file, const uint64_t offset, const size_t length, void *buf) {
    int err;

    // validate arguments
    if(!file || !length || !buf) {
        return -1;
    }

    // acquire lock
    err = mtx_lock(&gState.lock);
    if(err != thrd_success) return -1;

    // select best method
    if(TestFlags(gState.caps & ServerCaps::DirectIo)) {
        err = FileReadDirect(file, offset, length, buf);
    } else {
        fprintf(stderr, "no available read methods for file %08x!\n", file);
        err = -1;
    }

    // unlock
    mtx_unlock(&gState.lock);
    return err;
}
