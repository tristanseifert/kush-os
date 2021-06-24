#include "FAT.h"
#include "FAT32.h"
#include "Directory.h"
#include "File.h"

#include <algorithm>
#include <vector>

#include "Log.h"
#include "util/String.h"

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
            outErr = fat::FAT32::Alloc(partition.startLba, partition.size, disk, bpb, outFs);
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
 * Calculates the checksum over the short name in the given directory entry as outlined in the
 * Microsoft FAT specification.
 */
uint8_t FAT::CalculateShortNameChecksum(const DirEnt &ent) {
    uint8_t sum{0};
    auto bytes = reinterpret_cast<const uint8_t *>(&ent);

    for(size_t i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + bytes[i];
    }

    return sum;
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

/**
 * Reads the contents of the given cluster.
 *
 * @param cluster The cluster to read
 * @param outData Buffer to read data into
 * @param numSectors If non-zero, the number of sectors to read. Values larger than the volume's
 *        sectors per cluster value are ignored, and a full cluster is read.
 *
 * @return 0 on success, error code otherwise.
 */
int FAT::readCluster(const uint32_t cluster, std::vector<std::byte> &outData,
        const size_t numSectors) {
    const auto num = numSectors ? std::min(static_cast<size_t>(this->bpb.sectorsPerCluster),
            numSectors) : this->bpb.sectorsPerCluster;
    const auto sector = this->clusterToLba(cluster);
    return this->disk->Read(sector, num, outData);
}

/**
 * Reads the directory starting at the given cluster. The cluster chain is followed as for normal
 * files to determine when it ends.
 *
 * @param start First cluster of the directory; we'll follow the entire cluster chain.
 * @param outDir Pointer to write the allocated directory object
 * @param isRoot Whether this is the root directory; if so, we'll update the volume label if we
 *        find such a descriptor in the directory.
 *
 * @return 0 on success or an error code.
 */
int FAT::readDirectory(const uint32_t start, std::shared_ptr<fat::Directory> &outDir,
        const bool isRoot = false) {
    int err;

    bool isLast{false};
    uint32_t cluster{start};
    std::vector<std::byte> data;

    std::optional<LfnInfo> lfn;

    // create a directory
    auto dir = std::make_shared<fat::Directory>(start);

    // read all clusters belonging to the directory
    do {
        // read this cluster and interpret its directory entries
        data.clear();
        err = this->readCluster(cluster, data);
        if(err) return err;

        const std::span<std::byte> span = data;
        for(size_t i = 0; i < span.size() / sizeof(DirEnt); i++) {
            const auto entSpan = span.subspan(i * sizeof(DirEnt), sizeof(DirEnt));
            const auto &dirEnt = *reinterpret_cast<const DirEnt *>(entSpan.data());

            /*
             * Skip entry if it's free. Technically, the value of 0x00 indicates there are no more
             * directory entries to process for this directory, but a surprising amount of
             * filesystem tools screw this up so we treat it the same as the 0xE5 marker byte that
             * also indicates the entry is free.
             */
            if(dirEnt.name[0] == 0x00 || dirEnt.name[0] == 0xE5) continue;

            /**
             * Handle the long file name directory entries, if we've encountered one. These entries
             * ALWAYS precede the file that they are for, in descending order: so the first entry
             * we encounter is n | 0x40 (to indicate that it is the last name component), then n-1,
             * and so forth until we reach the long entry with a value of 1.
             */
            const auto type = dirEnt.attributes & DirEnt::Attributes::Mask;

            if(type == DirEnt::Attributes::LongFileName) {
                const auto &lfnEnt = *reinterpret_cast<const LfnDirEnt *>(entSpan.data());

                // start a new LFN (if this is the last entry)
                if(lfnEnt.lastLfn) {
                    lfn = LfnInfo{lfnEnt.order, lfnEnt.shortNameChecksum};
                    lfn->charBuf.resize(13 * lfnEnt.order, uint16_t{0x20});
                }
                // ensure the entry's checksum agrees with previously calculated checksum
                else {
                    if(lfnEnt.shortNameChecksum != lfn->shortNameChecksum) {
                        Warn("LFN checksum mismatch! Expected $%02x, read $%02x",
                                lfn->shortNameChecksum, lfnEnt.shortNameChecksum);
                        lfn.reset();
                        continue;
                    }
                }

                // insert the string data read out from it
                auto stringOff = lfn->charBuf.data() + ((lfnEnt.order - 1) * 13);
                std::copy(&lfnEnt.chars1[0], &lfnEnt.chars1[5], stringOff);
                std::copy(&lfnEnt.chars2[0], &lfnEnt.chars2[6], stringOff + 5);
                std::copy(&lfnEnt.chars3[0], &lfnEnt.chars3[2], stringOff + 11);

                lfn->readEntries++;
            }
            /**
             * Handle the "Volume ID" labels. This should technically only occur in the root
             * directory of the volume but we don't check that right now.
             *
             * XXX: Can LFN entries be used for the volume label?
             */
            else if(type & DirEnt::Attributes::VolumeId) {
                std::string temp;
                temp.append(&dirEnt.name[0], &dirEnt.name[8]);
                temp.append(&dirEnt.extension[0], &dirEnt.extension[2]);

                util::TrimTrailingWhitespace(temp);

                if(!isRoot) {
                    Warn("Encountered VolumeId outside root directory (is '%s')", temp.c_str());
                    break;
                }

                this->volumeLabel = temp;
            }
            // otherwise, it's a regular file entry
            else {
                std::string name;
                bool hasLfn{false};

                // get LFN name if available
                if(lfn) {
                    // ensure we've read all of the entries
                    auto &l = *lfn;
                    if(l.totalEntries == l.readEntries) {
                        // validate short name checksum
                        const auto cs = CalculateShortNameChecksum(dirEnt);
                        if(cs != lfn->shortNameChecksum) {
                            Warn("LFN checksum mismatch! Expected $%02x, calculated $%02x",
                                    lfn->shortNameChecksum, cs);
                            break;
                        }

                        // perform UCS-2 to UTF-8 conversion
                        auto temp = util::ConvertUcs2ToUtf8(l.charBuf);
                        util::TrimTrailingWhitespace(temp);
                        name = temp;
                        hasLfn = true;
                    }
                    // missing entries so force us to use the short filename
                    else {
                        Warn("Expected %u LFN entries but only got %u", l.totalEntries,
                                l.readEntries);
                    }
                }

                // if there was no long filename, use the short filename instead
                if(name.empty()) {
                    // get the first part of the name
                    name.append(&dirEnt.name[0], &dirEnt.name[8]);

                    // and extension if it's not all space
                    static const unsigned char kSpaces[] = {' ', ' ', ' '};
                    if(memcmp(&kSpaces, &dirEnt.extension, 3)) {
                        // XXX: is it allowed to skip this dot if name[0] is a dot?
                        if(dirEnt.name[0] != '.') {
                            util::TrimTrailingWhitespace(name);
                            name.push_back('.');
                        }

                        name.append(&dirEnt.extension[0], &dirEnt.extension[3]);
                    }

                    util::TrimTrailingWhitespace(name);
                }

                // create the directory entry
                auto ent = new fat::DirectoryEntry(dirEnt, name, hasLfn);
                dir->entries.push_back(ent);

                // prepare for next entry
                lfn.reset();
            }
        }

        // follow the cluster to get next
        err = this->getNextCluster(cluster, cluster, isLast);
        if(err) return err;
    } while(!isLast);

    // finished reading the entire directory
    outDir = dir;
    return 0;
}



/**
 * Read the directory referenced by the given directory entry.
 *
 * @param _dent Directory entry; must be the directort type.
 * @param outDir Pointer to store the read directory in
 *
 * @return 0 on success, error code otherwise
 */
int FAT::readDirectory(DirectoryEntryBase *_dent, std::shared_ptr<DirectoryBase> &outDir) {
    // validate entry
    if(_dent->getType() != DirectoryEntryBase::Type::Directory) return Errors::InvalidDirectoryEntry;

    auto dent = static_cast<fat::DirectoryEntry *>(_dent);
    if(!dent) return Errors::InvalidDirectoryEntry;

    // read it out
    std::shared_ptr<fat::Directory> dir;
    int err = this->readDirectory(dent->getFirstCluster(), dir);
    if(!err) outDir = dir;
    return err;
}

/**
 * Opens a file.
 */
int FAT::openFile(DirectoryEntryBase *_dent, std::shared_ptr<FileBase> &outFile) {
    // validate entry
    if(_dent->getType() != DirectoryEntryBase::Type::File) return Errors::InvalidDirectoryEntry;

    auto dent = static_cast<fat::DirectoryEntry *>(_dent);
    if(!dent) return Errors::InvalidDirectoryEntry;

    // try to create a file object from it
    std::shared_ptr<fat::File> file;
    int err = fat::File::Alloc(dent, this, file);
    if(!err) outFile = file;
    return err;
}

