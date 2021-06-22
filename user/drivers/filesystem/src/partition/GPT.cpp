#include "GPT.h"

#include "Log.h"

#include <algorithm>
#include <cstring>

#include <DriverSupport/disk/Client.h>
#include <checksum.h>

/**
 * Reads the sector that should contain the GPT header. We'll verify the first 8 bytes to see if
 * there may be a GPT, then try to read it in.
 */
int GPT::Probe(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        std::shared_ptr<PartitionTable> &outTable) {
    int err;
    std::vector<std::byte> data;

    // read the sector
    err = disk->Read(kHeaderLba, 1, data);
    if(err) return err;

    if(data.size() < GptHeader::kMagic.size() ||
       memcmp(data.data(), GptHeader::kMagic.data(), GptHeader::kMagic.size())) {
        return Errors::InvalidMagic;
    }

    // there's a header here so create a GPT object from it
    auto header = std::span<std::byte>(data).subspan(0, sizeof(GptHeader));
    std::shared_ptr<GPT> instance(new GPT(disk, header));

    if(!instance->status) {
        outTable = instance;
    }

    return instance->status;
}

/**
 * Initializes a GPT. This verifies the checksum of the header, reads out some values from it, then
 * reads the entire partition table list and verifies its checksum.
 *
 * The only condition that is true at this entry is that the magic value has been confirmed.
 */
GPT::GPT(const std::shared_ptr<DriverSupport::disk::Disk> &disk, const std::span<std::byte> &hdr) {
    int err;
    // get at the header and validate its size
    if(hdr.size() < sizeof(GptHeader)) {
        this->status = Errors::InvalidHeader;
        return;
    }
    const GptHeader &header = *reinterpret_cast<const GptHeader *>(hdr.data());

    if(header.headerSize < sizeof(GptHeader)) {
        this->status = Errors::InvalidHeader;
        return;
    }

    /*
     * Calculate the CRC32 of the header. This is over the bytes from the start of the header, up
     * to the byte indicated in the header size field. The CRC field is set to 0 for purposes of
     * the calculation, so we have to make a copy of the header.
     */
    std::vector<std::byte> checksumCopy;
    checksumCopy.reserve(header.headerSize);
    checksumCopy.insert(checksumCopy.end(), hdr.data(), hdr.data() + header.headerSize);

    uint32_t zero{0};
    memcpy(checksumCopy.data() + offsetof(GptHeader, headerChecksum), &zero, sizeof(zero));

    const auto crc = crc_32(reinterpret_cast<const uint8_t *>(checksumCopy.data()), checksumCopy.size());

    if(crc != header.headerChecksum) {
        this->status = Errors::HeaderChecksumMismatch;
        return;
    }

    if(header.revision < GptHeader::kMinRevision) {
        this->status = Errors::UnsupportedVersion;
        return;
    }

    // read some information
    this->diskId = header.diskId;

    this->usableLbas = {header.firstUsableLba, header.lastUsableLba};

    // TODO: read backup header

    // read the partition table entries
    err = this->readPartitionTable(disk, header);
    if(err) {
        this->status = err;
        return;
    }
}

/**
 * Reads the GPT partition table, and interprets each of its entries to determine which are valid
 * partitions that actually point to something.
 */
int GPT::readPartitionTable(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        const GptHeader &header) {
    int err;

    // ensure the table size is something we can make sense of
    if(header.partitionEntrySize < sizeof(GptPartition)) {
        return Errors::InvalidTableSize;
    }

    // figure out how many sectors the table is
    const auto sectorSize = disk->getSectorSize();
    const auto tableBytes = header.numPartitionEntries * header.partitionEntrySize;
    const auto tableSectors = (tableBytes + sectorSize - 1) / sectorSize;

    // read the tables
    std::vector<std::byte> tableData;

    err = disk->Read(header.partitionTableLba, tableSectors, tableData);
    if(err) return err;

    // calculate CRC
    const auto crc = crc_32(reinterpret_cast<const uint8_t *>(tableData.data()), tableBytes);
    if(crc != header.partitionEntryChecksum) {
        return Errors::TableChecksumMismatch;
    }

    // iterate over every entry
    for(size_t i = 0; i < header.numPartitionEntries; i++) {
        // get the entry
        const auto entryOffset = i * header.partitionEntrySize;
        auto entry = reinterpret_cast<GptPartition *>(tableData.data() + entryOffset);

        // ignore if the partition type is all 0's
        if(entry->partitionTypeGuid.isNil()) continue;

        // construct partition entry
        Partition p{entry->partitionTypeGuid, entry->lbaStart, (entry->lbaEnd - entry->lbaStart),
            entry->partitionUniqueGuid};

        const std::span<uint16_t> ucs2Name(entry->name, 32);
        p.name = ConvertUcs2ToUtf8(ucs2Name);

        this->partitions.emplace_back(std::move(p));
    }

    // success!
    return 0;
}



/**
 * Very quick and dirty UCS-2 to UTF-8 conversion.
 */
std::string GPT::ConvertUcs2ToUtf8(const std::span<uint16_t> &ucs2Str) {
    std::string str;

    for(const auto c : ucs2Str) {
        if(!c) break;

        if (c<0x80) str.push_back(c);
        else if (c<0x800) str.push_back(192+c/64), str.push_back(128+c%64);
        else if (c-0xd800u<0x800) goto error;
        else str.push_back(224+c/4096), str.push_back(128+c/64%64), str.push_back(128+c%64);
    }

    return str;

    // Invalid codepoint
error:;
    Warn("Invalid UCS-2 codepoint encountered");
    return "";
}
