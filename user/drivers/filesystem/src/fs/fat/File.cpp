#include "File.h"
#include "FAT.h"

#include "Log.h"

#include <algorithm>

using namespace fat;

/**
 * Allocate a file from the given FAT directory entry.
 */
int File::Alloc(DirectoryEntry *dent, FAT *fs, std::shared_ptr<File> &outFile) {
    outFile = std::make_shared<File>(dent, fs);
    return 0;
}

/**
 * Initializes the file object. This just gets the initial cluster and some other information from
 * the directory entry.
 */
File::File(DirectoryEntry *dent, FAT *_fs) : fs(_fs) {
    this->fileSize = dent->getFileSize();
    this->firstCluster = dent->getFirstCluster();

    this->name = dent->getName();
}

/**
 * Returns the path of this file, including ALL directories that lie above us. The root directory
 * is the root of the path.
 */
const std::string &File::getFullPath() const {
    // TODO: implement this
    return this->name;
}

/**
 * Perform the read IO.
 *
 * This will follow the cluster chain until the given offset and then read from that cluster (and
 * any subsequent ones) until the entire read has been satisfied, or we've reached the end of the
 * file.
 */
int File::read(const uint64_t offset, const size_t numBytes, std::vector<std::byte> &readBuf) {
    int err;
    std::vector<std::byte> temp;

    // XXX: should we expect the buffer be pre-cleared?
    readBuf.clear();

    // validate arguments
    if(!numBytes) {
        return 0;
    }
    // bail if we're going to be totally beyond the file
    else if(offset >= this->fileSize) {
        return 0;
    }

    // find the starting cluster
    const auto bytesPerCluster = this->fs->bpb.bytesPerSector * this->fs->bpb.sectorsPerCluster;
    uint32_t cluster = this->firstCluster;

    const auto startingCluster{offset / bytesPerCluster};
    if(startingCluster) {
        const size_t cacheOff = startingCluster-1;
        bool isLast{false};
        for(size_t i = 0; i < cacheOff; i++) {
            // XXX: handle better :)
            if(isLast) Abort("Got to end of cluster chain (%lu) for %lu byte read at %lu from %u byte file",
                    i, numBytes, offset, this->fileSize);

            err = this->fs->getNextCluster(cluster, cluster, isLast);
            if(err) return err;
        }
    }

    // figure out how many actual bytes left and the number of clusters to read
    const auto fileBytesLeft = this->fileSize - offset;
    const auto numBytesToRead = std::min(fileBytesLeft, numBytes);
    //const auto clustersToRead = (numBytesToRead + bytesPerCluster - 1) / bytesPerCluster;

    uint64_t currentOff = offset;
    auto bytesLeft = numBytesToRead;

    while(bytesLeft) {
        temp.clear();

        const auto clusterOff = currentOff % bytesPerCluster;
        const auto clusterBytes = std::min(bytesLeft, (bytesPerCluster - clusterOff));

        // read the cluster (TODO: support partial cluster reads)
        err = this->fs->readCluster(cluster, temp);
        if(err) return err;

        // extract the range we're after from it and copy out
        readBuf.insert(readBuf.end(), temp.data() + clusterOff,
                temp.data() + clusterOff + clusterBytes);

        // update bookkeeping
        bytesLeft -= clusterBytes;

        // read next cluster if needed
        if(bytesLeft) {
            bool isLast{false};
            err = this->fs->getNextCluster(cluster, cluster, isLast);
            if(err) return err;

            if(isLast) bytesLeft = 0;
        }
    }

    // finished reading
    return 0;
}

