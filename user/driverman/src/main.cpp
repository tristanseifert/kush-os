#include <string>
#include <thread>

#include "Log.h"

#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"

#include "db/DriverDb.h"
#include "forest/Forest.h"
#include "experts/Expert.h"
#include "rpc/MessageLoop.h"

const char *gLogTag = "driverman";

/**
 * Entry point for the driver manager
 */
int main(const int argc, const char **argv) {
    // parse arguments
    CLI::App app{"Driver manager"};

    std::string expertName;
    app.add_option("-e,--expert", expertName, "Platform expert to initialize");
    CLI11_PARSE(app, argc, argv);

    // load the driver database and set up forest
    Success("driverman starting (pexpert '%s')", expertName.c_str());

    Forest::init();
    MessageLoop::init();

    DriverDb::init();

    // create platform export
    auto expert = Expert::create(expertName);
    if(!expert) {
        Abort("Invalid platform expert: %s", expertName.c_str());
    }

    Trace("Beginning pexpert probe");
    expert->probe();

    // enter main message loop
    MessageLoop::the()->run();
}
