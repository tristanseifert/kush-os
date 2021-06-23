#include "Directory.h"
#include "FAT.h"

using namespace fat;

/**
 * Initializes a directory entry from the given FAT directory entry and decoded name. We'll read
 * out the file size and cluster locations.
 *
 * Since this type is used for both FAT16 and FAT32, we treat it as if we're on FAT32. This has the
 * side effect that the upper 16 bits of the cluster word might be garbage on FAT16, so you'll have
 * to consider this when using these objects.
 */
DirectoryEntry::DirectoryEntry(const FAT::DirEnt &ent, const std::string &_name) : name(_name) {
    // build up the attributes word
    using Attributes = FAT::DirEnt::Attributes;
    const auto type = ent.attributes & Attributes::Mask;

    if(type & Attributes::ReadOnly) {
        this->attributes |= DirectoryEntryAttributes::ReadOnly;
    }
    if(type & Attributes::Hidden) {
        this->attributes |= DirectoryEntryAttributes::Hidden;
    }

    this->size = ent.fileSize;
    this->firstCluster = ent.getCluster();
    this->isDirectory = (type & Attributes::Directory);
}



/**
 * Initializes the directory.
 */
Directory::Directory(const uint32_t _cluster) : cluster(_cluster) {

}

/**
 * Deletes all allocated directory entries.
 */
Directory::~Directory() {
    for(auto e : this->entries) {
        delete e;
    }
}

