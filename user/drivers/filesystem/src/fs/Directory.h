#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <sys/bitflags.hpp>

/**
 * Attributes that may be applied to a directory entry/file
 */
enum class DirectoryEntryAttributes: uintptr_t {
    None                                = 0,

    /// File is locked and read only
    ReadOnly                            = (1 << 0),
    /// Hide from "normal" directory listings
    Hidden                              = (1 << 1),
};
ENUM_FLAGS_EX(DirectoryEntryAttributes, uintptr_t);

/**
 * Abstract interface for a directory entry
 */
class DirectoryEntryBase {
    public:
        /// Different types of directory entries; may be extended
        enum class Type {
            Unknown, File, Directory, Other
        };

    public:
        virtual ~DirectoryEntryBase() = default;

        /**
         * Check if this directory entry matches the given name. This is should be implemented to
         * handle the filesystem's specific name comparison algorithms.
         */
        virtual bool compareName(const std::string_view &in) const = 0;

        /// Gets the type of entry
        virtual Type getType() const = 0;
        /// Gets the name of the entry
        virtual const std::string &getName() const = 0;
        /// Gets attributes associated with this entry
        virtual DirectoryEntryAttributes getAttributes() const = 0;

        /// Size, in bytes, for files
        virtual uint64_t getFileSize() const = 0;
};

/**
 * Abstract interface for a directory
 */
class DirectoryBase {
    public:
        virtual ~DirectoryBase() = default;

        /// Returns a reference to all entries in this directory
        virtual const std::vector<DirectoryEntryBase *> &getEntries() const = 0;

        /**
         * Look up the directory entry with the given name. This is done according to the name
         * comparison rules implemented by the directory entry items.
         *
         * @return Matching directory entry, or `nullptr` if not found
         */
        virtual DirectoryEntryBase *getEntry(const std::string_view &name) const = 0;
};
