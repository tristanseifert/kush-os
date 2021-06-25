#include "MessageLoop.h"
#include "LegacyIo.h"

#include "auto/Automount.h"
#include "fs/Filesystem.h"
#include "fs/File.h"

#include "util/Path.h"
#include "Log.h"

#include <rpc/rt/ServerPortRpcStream.h>

#include <cerrno>
#include <vector>

/**
 * Initializes the message loop. We'll create an RPC server IO stream that listens on a port and
 * registers it with the dispensary.
 */
MessageLoop::MessageLoop() :
    FilesystemServer(std::make_shared<rpc::rt::ServerPortRpcStream>(kPortName)) {
    this->legacy = std::make_unique<LegacyIo>(this);
}

/**
 * Shuts down the legacy style read interface thread.
 */
MessageLoop::~MessageLoop() {
    this->legacy.reset();
}

/**
 * Opens a file at the given path.
 *
 * We'll first determine what filesystem the path belongs to, then get the filesystem relative path
 * and traverse that filesystem's directories until we either fail to find a component, encounter a
 * file in place of a directory, or find the actual file.
 *
 * If the file was successfully found, a handle for further IO is created.
 */
MessageLoop::OpenFileReturn MessageLoop::implOpenFile(const std::string &path, uint32_t mode) {
    int err;
    if(kLogOpen) Trace("File to open: '%s' (mode $%x)", path.c_str(), mode);

    // get fs and fs relative path
    std::shared_ptr<Filesystem> fs;
    std::string fsPath;

    err = Automount::the()->getFsFor(path, fs, fsPath);
    if(err) {
        return { err };
    }

    // tokenize the path
    std::vector<std::string> components;
    util::SplitPath(fsPath, components);

    // iterate through the directories to the one we're after
    DirectoryEntryBase *fileDent{nullptr};
    std::shared_ptr<DirectoryBase> dir = fs->getRootDirectory();
    if(!dir) {
        return { Errors::InternalError };
    }

    for(size_t i = 0; i < (components.size() - 1); i++) {
        const auto &dirName = components[i];

        // get the entry
        auto entry = dir->getEntry(dirName);
        if(!entry) {
            return { ENOENT };
        }

        // recurse if it's a directory (it must be)
        if(entry->getType() != DirectoryEntryBase::Type::Directory) {
            return { ENOTDIR };
        }

        err = fs->readDirectory(entry, dir);
        if(err) {
            return { err };
        }
    }

    // read the file name from directory
    const auto &fileName = components.back();
    fileDent = dir->getEntry(fileName);

    if(!fileDent) {
        return { ENOENT };
    }

    // open the file and assign a handle
    std::shared_ptr<FileBase> file;
    err = fs->openFile(fileDent, file);
    if(err) {
        return { err };
    }

    const auto handle = this->nextFileHandle++;

    // store it and return the handle
    {
        std::lock_guard<std::mutex> lg(this->openFilesLock);
        this->openFiles.emplace(handle, file);
    }

    return { 0, handle, file->getFileSize() };
}

/**
 * Closes a previously opened file.
 */
int32_t MessageLoop::implCloseFile(uint64_t handle) {
    if(kLogOpen) Trace("Closing file handle $%08x", handle);

    std::lock_guard<std::mutex> lg(this->openFilesLock);

    // ensure handle exists
    if(!this->openFiles.contains(handle)) {
        return Errors::InvalidFileHandle;
    }

    // drop reference to it to close the file
    this->openFiles.erase(handle);
    return 0;
}

/**
 * Reads from a previously opened file.
 *
 * This copies the literal data rather than reading into a shared memory region, meaning this call
 * has a lot of overhead and shouldn't be used if reading large amounts of data.
 */
MessageLoop::SlowReadReturn MessageLoop::implSlowRead(uint64_t handle, uint64_t offset,
        uint16_t numBytes) {
    if(kLogIo) Trace("Read from file $%08x: offset %lu, %lu bytes", handle, offset, numBytes);

    // ensure handle exists
    std::shared_ptr<FileBase> file;
    {
        std::lock_guard<std::mutex> lg(this->openFilesLock);
        if(!this->openFiles.contains(handle)) {
            return { Errors::InvalidFileHandle };
        }
        file = this->openFiles[handle];
    }

    // perform the read
    std::vector<std::byte> temp;
    int err = file->read(offset, numBytes, temp);
    if(err) {
        return {err};
    }

    return { 0, temp };
}

