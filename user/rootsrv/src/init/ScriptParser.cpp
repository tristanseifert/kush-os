#include "ScriptParser.h"
#include "StringHelpers.h"

#include "log.h"

#include <cstring>

using namespace init;

/**
 * Parses the init script. All lines starting with comments or of only whitespace are ignored; then
 * we read the first token on the line to determine the type of line.
 *
 * If an error occurs, PANIC() is called.
 */
void ScriptParser::parse(std::shared_ptr<Bundle::File> file) {
    // clear out old state
    this->reset();

    // get at the string data
    auto contents = file->getContents();
    auto textPtr = reinterpret_cast<const char *>(contents.data());

    // read each line
    const char *linePtr = textPtr;
    std::string line;
    bool more = true;

    while(more) {
        // read the line
        const char *end = strchr(linePtr, '\n');
        if(end) {
            line = std::string(linePtr, (end - linePtr));
            linePtr = end+1;
        } else {
            line = std::string(linePtr, contents.size() - (linePtr - textPtr));
            more = false;
        }

        line = trim(line);

        // ignore empty lines
        if(line.empty()) {
            continue;
        }
        // ignore comments
        else if(line.find_first_of('#') == 0) {
            continue;
        }

        // get the keyword
        auto firstSpace = line.find_first_of(' ');
        std::string keyword = line;
        if(firstSpace != std::string::npos) {
            keyword = line.substr(0, firstSpace);
        }

        std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);

        // is this a line giving info about a server to start?
        if(!keyword.find("server1")) {
            auto args = std::string_view(line.data() + 8, line.length() - 8);
            this->processServer(args, false);
        }
        else if(!keyword.find("server2")) {
            auto args = std::string_view(line.data() + 8, line.length() - 8);
            this->processServer(args, true);
        }
        // ignore file directives
        else if(!keyword.find("file")) {
            // nothing
        }
        // unknown line
        else {
            LOG("Unhandled keyword '%s': '%s'", keyword.c_str(), line.c_str());
        }
    }
}

/**
 * Invokes the given function for each server we've read from the script.
 *
 * @param f A function to invoke for each server. Return true to continue iterating, or false to
 * terminate the iteration.
 * @param haveRootFs Whether servers that require the root fs or not are visited
 */
void ScriptParser::visitServers(std::function<bool(const std::string &,
            const std::vector<std::string> &)> const &f, const bool haveRootFs) {
    for(const auto &server : this->servers) {
        if(server.needsRootFs != haveRootFs) continue;

        if(!f(server.name, server.args)) {
            return;
        }
    }
}




/**
 * Processes a server line. The first field after the type token is the name of the server, which
 * is then followed by the argument string, where each space-separated item is a separate argument
 * to be passed to the server's main function.
 *
 * Note that arguments containing spaces can be escaped by surrounding them with quotes.
 */
void ScriptParser::processServer(const std::string_view &line, const bool postRootMount) {
    ServerInfo info{postRootMount};

    // get the server's name (the first token)
    auto firstSpace = line.find_first_of(' ');
    if(firstSpace != std::string::npos) {
        info.name = line.substr(0, firstSpace);
    } else {
        info.name = line;
    }

    // parse the arguments structure
    if(firstSpace != std::string::npos) {
        auto args = line;
        args.remove_prefix(std::min(firstSpace + 1, line.size()));

        // argv[0] is the binary name
        info.args.push_back(info.name);

        // parse all space-separated tokens out
        SplitStringArgs(args, info.args);
    }

    // insert the info struct
    this->servers.push_back(info);
}

