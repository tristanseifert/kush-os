#include <string>
#include <thread>

#include <getopt.h>

#include "Log.h"

#include "db/DriverDb.h"
#include "forest/Forest.h"
#include "experts/Expert.h"
#include "rpc/Server.h"

const char *gLogTag = "driverman";

/**
 * State as read in from command line
 */
static struct {
    /// Name of the expert to initialize
    std::string expertName;
} gState;

/**
 * Parse command line options
 */
static int ParseCommandLine(const int argc, char **argv) {
    int ch;

    /*
     * Define arguments for getopt_long()
     */
    static struct option options[] = {
        {"expert", required_argument, nullptr, 'e'},
        {nullptr, 0, nullptr, 0}
    };

    // iterate until all are options read
    while((ch = getopt_long(argc, argv, "", options, nullptr)) != -1) {

        switch(ch) {
            case 'e':
                gState.expertName = std::string(optarg);
                break;

            default:
                return -1;
        }
    }

    return 0;
}

/**
 * Entry point for the driver manager
 */
int main(const int argc, char **argv) {
    // parse arguments
    if(int err = ParseCommandLine(argc, argv)) {
        return err;
    }

    // load the driver database and set up forest
    Success("driverman starting (pexpert '%s')", gState.expertName.c_str());

    Forest::init();
    RpcServer::init();

    DriverDb::init();

    // create platform export
    auto expert = Expert::create(gState.expertName);
    if(!expert) {
        Abort("Invalid platform expert: %s", gState.expertName.c_str());
    }

    Trace("Beginning pexpert probe");
    expert->probe();

    // enter main message loop
    RpcServer::the()->run();
    Abort("RpcServer returned!");
}
