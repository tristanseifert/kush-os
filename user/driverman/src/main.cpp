#include <string>
#include <thread>

#include "Log.h"

#include "CLI/App.hpp"
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"

#include "forest/Forest.h"
#include "experts/Expert.h"

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

    // create platform export
    auto expert = Expert::create(expertName);
    if(!expert) {
        Abort("Invalid platform expert: %s", expertName.c_str());
    }

    Trace("Beginning pexpert probe");
    expert->probe();

    // enter main message loop
    while(1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
