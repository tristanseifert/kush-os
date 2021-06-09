#ifndef IDLPARSER_H
#define IDLPARSER_H

#include <memory>

class InterfaceDescription;

/**
 * Reads in IDL files and produces interface descriptions from them.
 */
class IDLParser {
    public:

        /// Attempt to parse the given file.
        std::shared_ptr<InterfaceDescription> parse(const std::string &filename);
};

#endif

