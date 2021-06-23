#include "FAT32.h"
#include "FAT.h"

#include "Log.h"

/**
 * Allocates a FAT32 filesystem. When called, we know that it is in fact FAT32; we just have to
 * set up a few structures at this point.
 */
FAT32::FAT32(const uint64_t start, const size_t length, 
        const std::shared_ptr<DriverSupport::disk::Disk> &disk, const std::span<std::byte> &data):
        FAT(start, length, disk, data) {
    // extract structs
    const auto extSpan = data.subspan(offsetof(Bpb, extended), sizeof(ExtendedBpb32));
    this->bpb32 = *(reinterpret_cast<const ExtendedBpb32 *>(extSpan.data()));

    // calculate some values
    const auto sec = this->bpb.bytesPerSector;
    const size_t rootDirSectors = ((this->bpb.numRootEntries * 32) + (sec - 1)) / sec;
    const size_t numDataSectors = this->bpb.numSectors32 - (this->bpb.numReservedSectors +
            (this->bpb.numFats * this->bpb32.tableSize32) + rootDirSectors);
    this->firstDataSector = this->bpb.numReservedSectors +
        (this->bpb.numFats * this->bpb32.tableSize32) + rootDirSectors;
    this->numClusters = numDataSectors / this->bpb.sectorsPerCluster;

    // read the root directory
    Trace("TODO: Read FAT32 root from cluster %lu (%lu)", this->bpb32.rootDirCluster,
            this->clusterToLba(this->bpb32.rootDirCluster));
}


/**
 * Allocation wrapper for a FAT32 filesystem
 */
int FAT32::Alloc(const uint64_t start, const size_t length,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        const std::span<std::byte> &bpb, std::shared_ptr<Filesystem> &out) {
    std::shared_ptr<FAT32> ptr(new FAT32(start, length, disk, bpb));
    if(!ptr->status) {
        out = ptr;
    }
    return ptr->status;
}

