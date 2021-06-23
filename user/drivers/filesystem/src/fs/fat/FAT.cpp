#include "FAT.h"
#include "FAT32.h"

#include <vector>

#include "Log.h"

/**
 * Checks if we can attach to the given partition. We'll verify if the GUID is one that matches,
 * then read the first sector (for the FAT header) and verify the header magic. If so, we're
 * reasonably certain the filesystem is valid so we initialize it.
 */
bool FAT::TryStart(const PartitionTable::Guid &id, const PartitionTable::Partition &partition,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk, std::shared_ptr<Filesystem> &outFs,
    int &outErr) {
    int err;

    // verify ID
    if(id != kBDPId) return false;
    // read the first sector (BPB)
    std::vector<std::byte> bpb;

    err = disk->Read(partition.startLba, 1, bpb);
    if(err) {
        outErr = err;
        return true;
    }

    const auto type = DetermineFatSize(bpb);
    if(type != Type::FAT32) {
        Warn("Unsupported FAT type: %d", type);
        outErr = Errors::UnsupportedFatType;
        return true;
    }

    // allocate the right type of FAT driver
    switch(type) {
        case Type::FAT32:
            outErr = FAT32::Alloc(partition.startLba, partition.size, disk, bpb, outFs);
            break;

        default:
            Abort("Unsupported FAT type: %d", type);
    }

    return true;
}

/**
 * Determines the type of FAT filesystem we're dealing with from reading its header.
 */
FAT::Type FAT::DetermineFatSize(const std::span<std::byte> &data) {
    // get just the base header
    const auto bpbSpan = data.subspan(0, sizeof(Bpb));
    const Bpb &bpb = *(reinterpret_cast<const Bpb *>(bpbSpan.data()));

    const auto extSpan = data.subspan(offsetof(Bpb, extended), sizeof(ExtendedBpb32));
    const ExtendedBpb32 &ext32 = *(reinterpret_cast<const ExtendedBpb32 *>(extSpan.data()));

    // calculate some values
    const size_t totalSectors = bpb.numSectors16 ? bpb.numSectors16 : bpb.numSectors32;
    const size_t fatSectors = bpb.tableSize16 ? bpb.tableSize16 : ext32.tableSize32;
    const size_t rootDirSectors = ((bpb.numRootEntries * 32) + (bpb.bytesPerSector - 1)) / bpb.bytesPerSector;
    const size_t numDataSectors = totalSectors - (bpb.numReservedSectors + (bpb.numFats * fatSectors) + rootDirSectors);
    const size_t totalClusters = numDataSectors / bpb.sectorsPerCluster;

    // determine the FAT type
    if(totalClusters < 4085) {
        return Type::FAT12;
    } else if(totalClusters < 65525) {
        return Type::FAT16;
    } else if(totalClusters < 268435445) {
        return Type::FAT32;
    }

    // technically, this would be an ExFAT volume
    return Type::Unknown;
}



/**
 * Initializes the FAT by reading the BPB out of the provided buffer.
 */
FAT::FAT(const uint64_t start, const size_t length,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk, const std::span<std::byte> &data):
        Filesystem(disk), startLba(start), numSectors(length) {
    const auto bpbSpan = data.subspan(0, sizeof(Bpb));
    this->bpb = *(reinterpret_cast<const Bpb *>(bpbSpan.data()));
}



/**
 * Converts a cluster number to a device absolute LBA.
 */
uint64_t FAT::clusterToLba(const uint32_t cluster) {
    return this->startLba + this->firstDataSector + ((cluster - 2) * this->bpb.sectorsPerCluster);
}
