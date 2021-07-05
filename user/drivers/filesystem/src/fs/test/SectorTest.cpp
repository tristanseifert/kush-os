#include "SectorTest.h"
#include "Log.h"
#include "fs/Filesystem.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>


/**
 * Checks if we can attach to the given partition. We'll verify if the GUID is one that matches and
 * if so create the filesystem object.
 */
bool SectorTestFs::TryStart(const PartitionTable::Guid &id, const PartitionTable::Partition &partition,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk, std::shared_ptr<Filesystem> &outFs,
    int &outErr) {
    // verify ID
    if(id != kTypeId) return false;

    // allocate FS
    outErr = SectorTestFs::Alloc(partition.startLba, partition.size, disk, outFs);
    return true;
}

/**
 * Attempt to allocate the sector testing fs.
 */
int SectorTestFs::Alloc(const uint64_t start, const size_t length,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk,
        std::shared_ptr<Filesystem> &out) {
    std::shared_ptr<SectorTestFs> ptr(new SectorTestFs(start, length, disk));
    if(!ptr->status) {
        out = ptr;
    }
    return ptr->status; 
}

/**
 * Sets up the sector test fs.
 */
SectorTestFs::SectorTestFs(const uint64_t start, const size_t length,
        const std::shared_ptr<DriverSupport::disk::Disk> &disk) : Filesystem(disk),
        startLba(start), numSectors(length){
    // start the test thread
    this->worker = std::make_unique<std::thread>(&SectorTestFs::workerMain, this);
}

/**
 * Shut down the worker boi. Setting the run flag to false is enough since it's sitting in an
 * infinite loop doing IO, and it'll check that flag when the IO completes.
 */
SectorTestFs::~SectorTestFs() {
    this->run = false;
    this->worker->join();
}

/**
 * Run loop for the driver tester.
 *
 * This will select random sectors to read from the disk (including random lengths) and verify that
 * they were read correctly. This requires that the entire partition is filled with sequentially
 * incrementing 4-byte integers in the native CPU byte order.
 */
void SectorTestFs::workerMain() {
    int err;

    // configurations
    constexpr static const size_t kMaxSectors{16};
    constexpr static const uint32_t kMaxSleepInterval{33}; // msec

    // set up
    std::vector<std::byte> data;

    Success("%s starting", __PRETTY_FUNCTION__);

    // enter run loop
    size_t loops{0};
    while(this->run) {
        data.clear();

        // generate a random sector and length pair
        const uint64_t sector = arc4random_uniform(this->numSectors) + this->startLba;
        const uint64_t sectorsLeft = this->numSectors - sector;
        const auto numSectors = std::min(sectorsLeft,
                static_cast<uint64_t>(arc4random_uniform(kMaxSectors)+1));

        // perform the read
        err = this->disk->Read(sector, numSectors, data);
        //Trace("Read from %7lu (%2lu) returned %d", sector, numSectors, err);

        if(err) {
            Abort("Read from %7lu (%2lu sectors) failed: %d (round %lu)", sector, numSectors, err,
                    loops);
        }

        // validate it
        const auto numWords = (numSectors * this->disk->getSectorSize()) / sizeof(uint32_t);
        const uint32_t startVal = ((sector - this->startLba) * this->disk->getSectorSize()) / sizeof(uint32_t);

        if(data.size() < (numWords * sizeof(uint32_t))) {
            Abort("Insufficient data read: got %lu bytes, need %lu", data.size(),
                    (numWords * sizeof(uint32_t)));
        }
        const auto ptr = reinterpret_cast<const uint32_t *>(data.data());

        for(size_t i = 0; i < numWords; i++) {
            const uint32_t expected = startVal + i;
            const auto actual = ptr[i];

            if(expected != actual) {
                uint32_t prev{0}, next{0};

                if(i) {
                    prev = ptr[i-1];
                }
                if(i != (numWords - 1)) {
                    next = ptr[i+1];
                }

                Abort("Mismatch at word %lu ($%05x): read $%08x, expected $%08x "
                        "prev $%08x, next $%08x "
                        "(disk read from sector %lu, %lu sectors)", i, i*sizeof(uint32_t),
                        actual, expected, prev, next, sector, numSectors);
            }
        }

        loops++;

        if(kMaxSleepInterval) {
            ThreadUsleep(1000 * (arc4random_uniform(kMaxSleepInterval)+1));
        }
    }

    // clean up
    Success("%s exiting", __PRETTY_FUNCTION__);
}
