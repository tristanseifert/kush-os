#include "Automount.h"

#include "Log.h"
#include "fs/Filesystem.h"

#include <toml++/toml.h>
#include <driver/DrivermanClient.h>
#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include <sys/syscalls.h>

Automount *Automount::gShared{nullptr};

/**
 * Initializes the shared instance of the automounter.
 */
void Automount::Init() {
    if(gShared) Abort("Cannot re-initialize automounter");
    gShared = new Automount;
}

/**
 * Initialize the automounter. This will read the automount configuration from disk.
 */
Automount::Automount() {
    if(!this->readConfig()) {
        Abort("Failed to read automount config; ensure initrd contains %s", kConfigPath.data());
    }

    if(!this->autos.contains("/")) {
        Abort("No automount info for root fs!");
    }
}

/**
 * Reads the automount configuration file.
 */
bool Automount::readConfig(const std::string_view &path) {
    // parse the config file
    toml::parse_result res = toml::parse_file(path);

    if(!res) {
        const auto &err = res.error();
        auto &beg = err.source().begin;
        Warn("Failed to parse automount config at %s %lu:%lu: %s", path.data(), beg.line,
                beg.column, err.description());
        return false;
    }

    // read out options
    const toml::table &tab = res;

    this->enable = res["automount"]["enabled"].value_or(true);

    // process each filesystem entry
    auto fs = tab["filesystem"].as_array();
    if(!fs) {
        Warn("Automount config at %s is invalid: %s", path.data(),
                "missing or invalid `filesystem` key");
        return false;
    }

    for(const auto &elem : *fs) {
        auto table = elem.as_table();
        if(!table) {
            Warn("Automount config at %s is invalid: %s", path.data(),
                    "invalid filesystem object type (expected table)");
            return false;
        }

        if(!this->processAutomountEntry(*table)) {
            return false;
        }
    }

    return true;
}

/**
 * Processes an entry in the `filesystem` array in the automount config. Each of these entries
 * defines a single mount point with a single match descriptor.
 */
bool Automount::processAutomountEntry(const toml::table &tbl) {
    // get automount path
    const std::string_view path = tbl["path"].value_or("");
    if(path.empty()) {
        return false;
    }

    // decode the match object
    const auto &match = tbl["match"].as_table();
    if(!match) {
        return false;
    }

    // currently, we can only match by disk/partition
    return this->diskPartAutomount(*match, path);
}

/**
 * Parses a disk path/partition match descriptor for an automount partition.
 */
bool Automount::diskPartAutomount(const toml::table &tbl, const std::string_view &vfsPath) {
    using namespace std::literals;

    // read the keys
    const auto diskPath = tbl["disk"].value_or(""sv);
    const auto partition = tbl["partition"].value_or(0U);

    if(diskPath.empty() || !partition) {
        return false;
    }

    // create the automount match
    AutoInfo info{std::string(diskPath), partition-1};
    this->autos.emplace(std::string(vfsPath), std::move(info));

    Trace("Mounting partition %lu on %s at %s", partition, diskPath.data(), vfsPath.data());

    return true;
}



/**
 * Processes a newly started filesystem.
 */
void Automount::startedFs(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        const PartitionTable::Partition &p, const std::shared_ptr<Filesystem> &fs) {
    // check each of the automount infos
    for(const auto &[vfsPath, info] : this->autos) {
        if(!info.match(disk, p, fs)) continue;
        this->filesystems.emplace(vfsPath, fs);

        Trace("Mounted fs %p (label %s) at %s", fs.get(),
                fs->getVolumeLabel().value_or("(none)").c_str(), vfsPath.c_str());

        // send out notifications for root fs mount
        if(vfsPath == "/") {
            this->needsRootFsNotify = true;
        }
    }
}

/**
 * Notifies the root server (to disable the init file IO service) and the driver manager (to load
 * the larger, more comprehensive on-disk driver database) that the root filesystem has just become
 * available.
 */
void Automount::sendRootMountedNotes() {
    int err;

    // allocate a message bvuffer
    void *buf{nullptr};
    err = posix_memalign(&buf, 16, sizeof(rpc::RpcPacket) + 16);
    if(err) {
        Abort("%s failed: %d", "posix_memalign", err);
    }

    // shut down the init file handler
    constexpr static const uint32_t kInitFileShutdownMessage{0x48b9ef0a};
    constexpr static const std::string_view kInitFilePortName{"me.blraaz.rpc.rootsrv.initfileio"};

    uintptr_t initPort, dyldosrvPort;

    err = LookupService(kInitFilePortName.data(), &initPort);
    if(err == 1) {
        auto packet = reinterpret_cast<rpc::RpcPacket *>(buf);
        memset(packet, 0, sizeof(*packet));
        packet->type = kInitFileShutdownMessage;

        err = PortSend(initPort, buf, sizeof(rpc::RpcPacket));
        if(err) {
            Warn("%s failed: %d", "PortSend", err);
        }
    } else {
        Warn("Failed to resolve %s port: %d", "init file", err);
    }

    ThreadUsleep(1000 * 100);

    // notify dyldosrv (defined as DyldosrvMessageType::RootFsUpdated)
    constexpr static const uint32_t kDyldosrvRootUpdateMessage{'FSUP'};
    constexpr static const std::string_view kDyldosrvPortName{"me.blraaz.rpc.dyldosrv"};

    err = LookupService(kDyldosrvPortName.data(), &dyldosrvPort);
    if(err == 1) {
        auto packet = reinterpret_cast<rpc::RpcPacket *>(buf);
        memset(packet, 0, sizeof(*packet));
        packet->type = kDyldosrvRootUpdateMessage; 

        err = PortSend(dyldosrvPort, buf, sizeof(rpc::RpcPacket));
        if(err) {
            Warn("%s failed: %d", "PortSend", err);
        }
    } else {
        Warn("Failed to resolve %s port: %d", "dyldosrv", err);
    } 

    // notify driverman
    auto driverman = libdriver::RpcClient::the();
    err = driverman->NotifyDriverman(libdriver::RpcClient::NoteKeys::RootFsUpdated);

    if(err) {
        Warn("Failed to send driverman a root fs updated notification: %d", err);
    }

    // TODO: notify others
    Success("TODO: send root mount notifications");

    // clean up
    free(buf);
}

/**
 * Checks if the automount info matches the given disk/partition.
 */
bool Automount::AutoInfo::match(const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        const PartitionTable::Partition &p, const std::shared_ptr<Filesystem> &fs) const {
    return disk->getForestPath() == this->diskForestPath && p.index == this->diskPartition;
}



/**
 * Find the filesystem that has the longest match in the vfs path against the provided path; this
 * means it's the most specific filesystem and we trim that part off the file's path to get the
 * filesystem specific path.
 *
 * If no entries can match, we'll look on the root filesystem.
 */
int Automount::getFsFor(const std::string_view &path, std::shared_ptr<Filesystem> &outFs,
        std::string &outFsPath) {
    // TODO: implement the searching

    // return the root fs
    outFs = this->filesystems["/"];
    outFsPath = path;

    return 0;
}

/**
 * Sends the root fs available notifications if needed. This is done after all filesystems have
 * been automounted, so that if there are any secondary data filesystems, they'll also have been
 * mounted.
 */
void Automount::postMount() {
    if(this->needsRootFsNotify) {
        this->sendRootMountedNotes();
        this->needsRootFsNotify = false;
    }
}

