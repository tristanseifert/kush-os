#ifndef INIT_SCRIPTPARSER_H
#define INIT_SCRIPTPARSER_H

#include "Bundle.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace init {
/**
 * Parses a boot-up initialization script and extracts from it the information required to continue
 * setting up the system.
 *
 * For now, this just extracts the list of servers to load.
 */
class ScriptParser {
    public:
        /// Parses an init script.
        void parse(std::shared_ptr<Bundle::File> file);

        /// Visits all servers
        void visitServers(std::function<bool(const std::string &, const std::vector<std::string> &)> const &f);

        /// Clears all internal state.
        void reset() {
            this->servers.clear();
        }

    private:
        /// info on a server to launch
        struct ServerInfo {
            /// server binary name (under /sbin; or full path if it contains a slash)
            std::string name;
            /// any arguments
            std::vector<std::string> args;
        };

    private:
        void processServer(const std::string_view &line);

    private:
        /// servers to be launched
        std::vector<ServerInfo> servers;
};
}

#endif
