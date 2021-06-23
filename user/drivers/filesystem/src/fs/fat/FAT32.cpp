#include "FAT32.h"
#include "FAT.h"
#include "Directory.h"

#include "Log.h"

#include <algorithm>
#include <cstring>
#include <optional>

using namespace fat;

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
    this->status = this->readDirectory(this->bpb32.rootDirCluster, this->root, true);
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



/**
 * Reads a sector of the FAT.
 */
int FAT32::readFat(const size_t n, std::vector<std::byte> &out) {
    if(n >= this->bpb32.tableSize32) return Errors::FatSectorOutOfRange;
    return this->disk->Read(this->startLba + this->bpb.numReservedSectors + n, 1, out);
}

/**
 * Reads the FAT to determine the next cluster following this one is.
 *
 * We'll see if the sector of the FAT we're after is in our cache; if so, we avoid performing the
 * read under the assumption that the cached copy is updated with any writes.
 */
int FAT32::getNextCluster(const uint32_t _cluster, uint32_t &outNextCluster, bool &outIsLast) {
    int err;
    uint32_t entry;

    // the top 4 bits of cluster values are reserved
    const uint32_t cluster{_cluster & 0x0FFFFFFF};

    // pull sector from cache, or perform read if needed
    const size_t fatByteOff{cluster * 4};
    const size_t fatSector{fatByteOff / this->bpb.bytesPerSector};
    const auto byteOff{fatByteOff % this->bpb.bytesPerSector};

    if(this->fatPageCache.contains(fatSector)) {
        const auto &sector = this->fatPageCache[fatSector];
        memcpy(&entry, sector.data() + byteOff, sizeof(entry));
    } else {
        std::vector<std::byte> sector;
        err = this->readFat(fatSector, sector);
        if(err) return err;

        if(kLogFatCache) Trace("Cached FAT sector %lu", fatSector);

        this->fatPageCache[fatSector] = sector;
        memcpy(&entry, sector.data() + byteOff, sizeof(entry));
    }

    // handle the value
    if(kLogFatTraversal) Trace("FAT %08x -> %08x", cluster, entry);

    outIsLast = (entry >= 0x0FFFFFF8);
    if(entry < 0x0FFFFFF8) {
        outNextCluster = entry & 0x0FFFFFFF;
    }

    return 0;
}

/**
 * Returns a reference to the root directory.
 */
std::shared_ptr<DirectoryBase> FAT32::getRootDirectory() const {
    return std::static_pointer_cast<DirectoryBase>(this->root);
}
