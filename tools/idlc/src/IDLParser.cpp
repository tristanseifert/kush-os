#include "IDLParser.h"
#include "IDLGrammar.h"

#include <iostream>

#include <tao/pegtl.hpp>

/**
 * Attempts to parse an interface description out of the given file.
 *
 * Any parse errors are caught internally here, and printed to standard error stream. Other errors,
 * such as file IO, are not caught and are expected to be dealt with by the caller.
 *
 * @return A parsed interface description, or `nullptr` if parsing failed.
 */
std::shared_ptr<InterfaceDescription> IDLParser::parse(const std::string &filename) {
    using namespace tao;

    // open the input
    std::cout << "* Parsing: " << filename << std::endl;
    pegtl::file_input in(filename);

    // then try to parse it
    try {
        if(!pegtl::parse<idl::grammar>(in)) {
            std::cerr << "Local parsing error" << std::endl;
            return nullptr;
        }
    }
    // handle parse exceptions
    catch(const pegtl::parse_error &e) {
        const auto p = e.positions().front();
        std::cerr << e.what() << '\n'
                  << in.line_at(p) << '\n'
                  << std::setw(p.column) << '^' << std::endl;
    }

    // return parsed object
    return nullptr;
}

