#include "Driver.h"
#include "Log.h"

#include <rpc/task.h>
#include <driver/base85.h>

#include <algorithm>
#include <system_error>
#include <new>

/**
 * Creates a driver whose binary is located at the given path.
 */
Driver::Driver(const std::string &_path) : path(_path) {

}

/**
 * Deletes all memory we allocated.
 */
Driver::~Driver() {
    for(auto match : this->matches) {
        delete match;
    }
}


/**
 * Adds a match object.
 */
void Driver::addMatch(libdriver::DeviceMatch *match) {
    this->matches.push_back(match);
}

/**
 * Tests whether any or all of the match structures provided match this driver's.
 */
bool Driver::test(const std::span<libdriver::DeviceMatch *> &matches, const bool oand) {
    // TODO: implement the AND operation
    if(oand) {
        return false;
    }

    // iterate over all matches
    for(auto &match : this->matches) {
        auto m = std::any_of(matches.begin(), matches.end(), [&](const auto &in) {
            return in->matches(match);
        });
        if(m) {
            return true;
        }
    }

    // no matches
    return false;
}



/**
 * Instantiates an instance of the driver. Depending on the configuration, this will always spawn a
 * new task, or spawn one task and send messages for further instantiations to it.
 */
void Driver::start(const std::span<std::byte> &auxData) {
    int err;

    // first instance OR always launch a new instance
    if(this->taskHandles.empty() || this->alwaysLaunchNew) {
        // serialize aux data and build args
        char *argPtr = nullptr;
        const char *params[3] = {
            this->path.c_str(),
            // if we have aux params, write them here
            nullptr,
            // end of list
            nullptr,
        };

        if(!auxData.empty()) {
            // allocate a buffer: every 4 bytes input -> 5 chars output
            const auto outSize = (((auxData.size() + 4 - 3) / 4) * 6) + 8;
            argPtr = reinterpret_cast<char *>(malloc(outSize));
            if(!argPtr) {
                throw std::bad_alloc();
            }

            memset(argPtr, 0, outSize);

            // do base85 encoding
            bintob85(argPtr, auxData.data(), auxData.size());
            params[1] = argPtr;
        }

        Trace("Launching driver %s", this->path.c_str());

        // launch the task
        uintptr_t handle;
        err = RpcTaskCreate(this->path.c_str(), params, &handle);

        if(argPtr) {
            // release the param buffer
            free(argPtr);
        }

        if(err) {
            throw std::system_error(err, std::system_category(), "RpcTaskCreate");
        }
    }
    // notify existing instance
    else {
        Warn("%s unimplemented", __PRETTY_FUNCTION__);
    }
}
