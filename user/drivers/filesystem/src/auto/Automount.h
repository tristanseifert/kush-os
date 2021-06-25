#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <DriverSupport/disk/Client.h>
#include <toml++/toml.h>

#include "partition/PartitionTable.h"

class Filesystem;

/**
 * Filesystems can be automatically attached (mounted) to a particular virtual path in the
 * filesystem hierarchy. This is configured by a configuration file (see /config/Automount.toml)
 * that is loaded from the initial RAM disk.
 *
 * The configuration of the automounter can be updated dynamically.
 */
class Automount {
    /// Default path for the automounter configuration file
    constexpr static const std::string_view kConfigPath{"/config/Automount.toml"};

    public:
        static void Init();

        /// Returns the shared instance of the automounter
        static Automount *the() {
            return gShared;
        }

        /// A filesystem has been detected and initialized on the given disk
        void startedFs(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const PartitionTable::Partition &p, const std::shared_ptr<Filesystem> &fs);

        /// Get filesystem that contains the given path, and the fs relative path.
        int getFsFor(const std::string_view &path, std::shared_ptr<Filesystem> &outFs,
                std::string &outFsPath);

        /// Sends any mount notifications once all filesystems have been automounted.
        void postMount();

    private:
        /**
         * Describes information on a filesystem to automount.
         */
        struct AutoInfo {
            /// Disk on which the filesystem resides
            std::string diskForestPath;
            /// Partition number on said disk (zero based)
            size_t diskPartition;

            /// Check whether this automount info matches a newly started FS
            bool match(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
                const PartitionTable::Partition &p, const std::shared_ptr<Filesystem> &fs) const;
        };

    private:
        Automount();

        bool readConfig(const std::string_view &path = kConfigPath);
        [[nodiscard]] bool processAutomountEntry(const toml::table &);
        [[nodiscard]] bool diskPartAutomount(const toml::table &, const std::string_view &);

        void sendRootMountedNotes();

    private:
        static Automount *gShared;

        /// Set when automounting is enabled
        bool enable{true};
        /// When set, we need to notify that the root fs has become available
        bool needsRootFsNotify{false};

        /// Mapping of vfs path -> automount info. Checked for every new fs
        std::unordered_map<std::string, AutoInfo> autos;
        /// Instantiated filesystems to their root paths
        std::unordered_map<std::string, std::shared_ptr<Filesystem>> filesystems;
};
