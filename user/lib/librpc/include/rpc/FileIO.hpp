/*
 * RPC interface for file-based IO.
 *
 * This interface is implemented by various different servers; before making the first request to
 * perform any IO, you should do a GetCapabilities request to see what calls you can make for
 * actual file IO.
 */
#ifndef RPC_SYSTEM_FILEIO_HPP
#define RPC_SYSTEM_FILEIO_HPP

#include <cstdint>
#include <cstddef>

#include <sys/bitflags.hpp>

namespace rpc {
/**
 * Message type
 */
enum class FileIoEpType: uint32_t {
    // flag to indicate a reply; since we use 4cc's, the high bit is available
    ReplyFlag                           = 0x80000000,

    GetCapabilities                     = 'CAPG',
    GetCapabilitiesReply                = GetCapabilities | ReplyFlag,

    OpenFile                            = 'OPEN',
    OpenFileReply                       = OpenFile | ReplyFlag,
    CloseFile                           = 'CLOS',
    CloseFileReply                      = CloseFile | ReplyFlag,

    WriteFileDirect                     = 'WRIT',
    WriteFileDirectReply                = WriteFileDirect | ReplyFlag,
    ReadFileDirect                      = 'READ',
    ReadFileDirectReply                 = ReadFileDirect | ReplyFlag,
};

/**
 * Capabilities that may be supported by a file endpoint.
 */
ENUM_FLAGS_EX(FileIoCaps, uint32_t);
enum class FileIoCaps: uint32_t {
    /// Direct IO is supported
    DirectIo                            = (1 << 0),
};
/**
 * Request for the capabilities of the file IO endpoint
 */
struct FileIoGetCaps {
    /// requested protocol version; this should always be 1
    uint32_t requestedVersion;
};
/**
 * Reply to the capabilities request.
 */
struct FileIoGetCapsReply {
    /// endpoint protocol version
    uint32_t version;
    /// supported capabilities mask
    FileIoCaps capabilities;
    /// maximum read block size, or 0 if unlimited
    uint32_t maxReadBlockSize;
};


/**
 * Flags to determine the actions to perform when opening a file.
 */
ENUM_FLAGS_EX(FileIoOpenFlags, uint32_t);
enum class FileIoOpenFlags: uint32_t {
    /// Open for reading only
    ReadOnly                            = (1 << 0),
    /// Open for writing only
    WriteOnly                           = (1 << 1),
    /// Open for reading and writing
    ReadWrite                           = (ReadOnly | WriteOnly),

    /// Create the file if it doesn't exist
    CreateIfNotExists                   = (1 << 4),

    /// Acquire an exclusive lock on the file.
    LockExclusive                       = (1 << 8),
    /// Acquire a shared lock on the file
    LockShared                          = (1 << 9),
};

/**
 * Request to open a file
 */
struct FileIoOpen {
    /// open modes
    FileIoOpenFlags mode;

    /// length of path (max 64K bytes. in practice much lower). doesn't include 0 terminator
    uint16_t pathLen;
    /// UTF-8 path string
    char path[];
};
/**
 * Response to an open file request
 *
 * This contains a "file handle," which is an opaque token that identifies the file, when used in
 * RPC requests from the task that created it. You should not rely on them taking any particular
 * form in code, only that they exist.
 *
 * The handle will be around until the task that created it closes it, or until it terminates. The
 * file IO handler should add itself as an observer on the task's port to be notified when it
 * terminates so that file handles aren't leaked if tasks terminate unexpectedly.
 */
struct FileIoOpenReply {
    /// Status code, if 0 the file was opened, any negative value indicates an error
    int32_t status;
    /**
     * The same flags as from the request, but with the bits that weren't considered masked off.
     * For example, if the file was requested with the "CreateIfNotExists" flag, but it exists, the
     * bit will be clear here.
     */
    FileIoOpenFlags flags;
    /// if the file was opened, a handle representing it that can be used to perform IO
    uintptr_t fileHandle;

    /// length of the file, in bytes
    uint64_t length;
};

/**
 * Close a previously opened file.
 */
struct FileIoClose {
    /// file handle to close
    uintptr_t file;
};
/**
 * Response to a file close request.
 */
struct FileIoCloseReply {
    /// status code: 0 indicates success
    int32_t status;
};



/**
 * Request to read from a file
 */
struct FileIoReadReq {
    /// file handle
    uintptr_t file;

    /// offset to start reading from
    uint64_t offset;
    /// number of bytes to read
    uint64_t length;
};
/**
 * Read request reply
 */
struct FileIoReadReqReply {
    /// file handle that this read request belongs to
    uintptr_t file;
    /// status code: 0 indicates at least one byte was read
    int32_t status;

    /// number of bytes of data returned
    size_t dataLen;
    /// actual data
    char data[];
};

}

#endif
