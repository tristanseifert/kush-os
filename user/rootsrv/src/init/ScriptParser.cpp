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

        // is this a line giving info about a server to start?
        if(line.starts_with("SERVER ")) {
            auto args = std::string_view(line.data() + 7, line.length() - 7);
            this->processServer(args);
        }
        // ignore file directives
        else if(line.starts_with("FILE ")) {
            // nothing
        }
        // unknown line
        else {
            LOG("Unhandled line: '%s'", line.c_str());
        }
    }
}

/**
 * Invokes the given function for each server we've read from the script.
 *
 * @param f A function to invoke for each server. Return true to continue iterating, or false to
 * terminate the iteration.
 */
void ScriptParser::visitServers(std::function<bool(const std::string &, const std::vector<std::string> &)> const &f) {
    for(const auto &server : this->servers) {
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
void ScriptParser::processServer(const std::string_view &line) {
    ServerInfo info;

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

        // parse all space-separated tokens out
        SplitStringArgs(args, info.args);
    }

    // insert the info struct
    this->servers.push_back(info);
}

