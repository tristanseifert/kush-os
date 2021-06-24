#pragma once

#include "FAT.h"
#include "fs/Directory.h"

#include <cstdint>
#include <memory>
#include <vector>

class FAT;

namespace fat {
/**
 * Represents a FAT directory entry. Its information is copied from an old style directory entry
 * object; its name is specified separately since the long file name is decoded by the FAT entry
 * reader itself.
 */
class DirectoryEntry: public ::DirectoryEntryBase {
    public:
        DirectoryEntry(const FAT::DirEnt &ent, const std::string &name, const bool hasLfn);

        /**
         * FAT filesystems can only contain files or directories. There is no support for any other
         * type of special file, so this becomes a binary choice.
         */
        Type getType() const override {
            return this->isDirectory ? DirectoryEntryBase::Type::Directory :
                DirectoryEntryBase::Type::File;
        }
        /// Return the filename of the entry
        const std::string &getName() const override {
            return this->name;
        }
        /// Get the file attributes
        DirectoryEntryAttributes getAttributes() const override {
            return this->attributes;
        }
        /// Return the file size
        uint64_t getFileSize() const override {
            return this->size;
        }

        /// Return the FAT cluster this directory starts at
        constexpr inline auto getFirstCluster() const {
            return this->firstCluster;
        }

        /// Perform FAT name comparison
        bool compareName(const std::string_view &in) const override;

    private:
        /// Full long filename (if available)
        std::string name;

        /// Is this file pointing to a directory?
        bool isDirectory{false};
        /// Did we have a long file name for this item?
        bool hasLfn{false};
        /// Size of file (4GB max on FAT)
        uint32_t size{0};

        /// First cluster for the file contents
        uint32_t firstCluster{0};

        DirectoryEntryAttributes attributes{DirectoryEntryAttributes::None};
};

/**
 * Represents a FAT directory. This contains some extra information (like the cluster from which
 * the directory was read) in addition to just directory entries.
 */
class Directory: public ::DirectoryBase {
    friend class ::FAT;

    public:
        Directory(const uint32_t cluster);
        virtual ~Directory();

        /// Returns a reference to our directory entries array.
        const std::vector<DirectoryEntryBase *> &getEntries() const override {
            return this->entries;
        }

        DirectoryEntryBase *getEntry(const std::string_view &name) const override;

    private:
        /// Starting cluster of the directory
        uint32_t cluster{0};

        /**
         * All directory entries in the directory, in the order they were read from the directory
         * on the disk.
         */
        std::vector<DirectoryEntryBase *> entries;
};
} // namespace fat
