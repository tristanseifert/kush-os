#ifndef IDLPARSER_H
#define IDLPARSER_H

#include <memory>
#include <vector>

class InterfaceDescription;

/**
 * Reads in IDL files and produces interface descriptions from them.
 */
class IDLParser {
    using IDPointer = std::shared_ptr<InterfaceDescription>;

    public:
        /// Attempt to parse the given file, adding any created interface descriptors
        bool parse(const std::string &filename, std::vector<IDPointer> &outInterfaces);
};

#endif

