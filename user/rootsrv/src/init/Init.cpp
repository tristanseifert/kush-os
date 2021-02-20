#include "Init.h"
#include "Bundle.h"
#include "ScriptParser.h"

#include "log.h"

#include <string>
#include <vector>

#include <sys/syscalls.h>

using namespace init;

static void InitServer(Bundle &bundle, const std::string &name,
        const std::vector<std::string> &params);

/**
 * Parses the init script to discover all servers, then initializes them in the specified order.
 */
void init::SetupServers(Bundle &bundle) {
    // get the script file
    auto scriptFile = bundle.open("/etc/default.init");
    REQUIRE(scriptFile, "failed to open default init script");

    // parse it
    init::ScriptParser script;
    script.parse(scriptFile);

    script.visitServers([&](const auto &name, const auto &params) {
        InitServer(bundle, name, params);
        return true;
    });

}


/**
 * Initializes a server. This loads the binary from the init bundle, either from /sbin (if the
 * name parameter is not a path, i.e. contains no slashes) or by interpreting it as an absolute
 * path. The provided parameters are passed to the server's main function.
 */
static void InitServer(Bundle &bundle, const std::string &name,
        const std::vector<std::string> &params) {
#ifndef NDEBUG
    LOG("Initializing server '%s'", name.c_str());
#endif

    // determine the path
    auto path = name;
    if(path.find_first_of('/') == std::string::npos) {
        path = "/sbin/" + name;
    }

    // try to get the server binary
    auto file = bundle.open(path);
    REQUIRE(file, "Failed to load server '%s' (from %s)", name.c_str(), path.c_str());

    // TODO: create task
    for(const auto &str : params) {
        LOG("Param: '%s'", str.c_str());
    }

    // clean up
    delete file;
}

