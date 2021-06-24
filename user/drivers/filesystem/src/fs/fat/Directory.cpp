#include "Directory.h"
#include "FAT.h"

#include "Log.h"

#include <cstring>

using namespace fat;

/**
 * Initializes a directory entry from the given FAT directory entry and decoded name. We'll read
 * out the file size and cluster locations.
 *
 * Since this type is used for both FAT16 and FAT32, we treat it as if we're on FAT32. This has the
 * side effect that the upper 16 bits of the cluster word might be garbage on FAT16, so you'll have
 * to consider this when using these objects.
 */
DirectoryEntry::DirectoryEntry(const FAT::DirEnt &ent, const std::string &_name,
        const bool _hasLfn) : name(_name), hasLfn(_hasLfn) {
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
 * Performs a name comparison.
 *
 * The FAT specification indicates that all name comparisons should be done case insensitive. It
 * also indicates there's some unique behavior to do with short vs. long names, but since we do not
 * differentiate between them (and in fact ignore this entirely when reading directories: if an
 * entry has a long name, its short name is _not_ recorded) we ignore that also.
 *
 * This _should_ use something better than strcasecmp... that will break for any multibyte UTF-8
 * characters (e.g. non-ASCII)
 *
 * @return Whether the name matches.
 */
bool DirectoryEntry::compareName(const std::string_view &in) const {
    // check string length first
    if(in.length() != this->name.length()) return false;

    // perform case insensitive compare
    return !strncasecmp(in.data(), this->name.data(), in.length());
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

/**
 * Tests each of the entries to see if any of them match. This is a very naieve way and sucks for
 * large directories but FAT is going to suck eggs performance wise for large directories anyways
 * so it's kind of a moot point
 */
DirectoryEntryBase *Directory::getEntry(const std::string_view &name) const {
    for(auto p : this->entries) {
        if(p->compareName(name)) return p;
    }

    return nullptr;
}
