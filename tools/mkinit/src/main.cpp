#include "InitBundle.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

std::string & ltrim(std::string & str) {
    auto it2 =  std::find_if(str.begin(), str.end(), [](char ch){ return !std::isspace<char>(ch, std::locale::classic()); });
    str.erase(str.begin(), it2);
    return str;
}
std::string & rtrim(std::string & str) {
    auto it1 =  std::find_if(str.rbegin(), str.rend(), [](char ch){ return !std::isspace<char>(ch, std::locale::classic()); });
    str.erase(it1.base(), str.end());
    return str;
}
std::string & trim(std::string & str) {
    return ltrim(rtrim(str));
}

/**
 * Input state
 */
std::string gScriptPath, gOutPath, gSysroot;

/**
 * Parse the command line. The tool should be invoked as "mkinit [flags] -i <script> -o <output path>" 
 * where the optional flags can be any of the following:
 *
 * -s [path]: Specifies a sysroot to prepend to all paths in the init script.
 *
 * @return true if the program execution should continue, false otherwise.
 */
static bool ParseCommandline(int argc, char *const *argv) {
    int option;
    while((option = getopt(argc, argv, ":i:o:s:")) != -1) {
        switch(option) {
            // script file
            case 'i':
                gScriptPath = std::string(optarg);
                break;
            // output binary file
            case 'o':
                gOutPath = std::string(optarg);
                break;

            case 's':
                gSysroot = std::string(optarg);
                break;

            // unknown option
            case '?':
                std::cerr << "unknown option " << (char) optopt << std::endl;
                return false;
        }
    }

    // ensure the script path and output path were specified
    if(gScriptPath.empty() || gOutPath.empty()) {
        return false;
    }

    // if we get here, we were successful
    return true;
}

/**
 * Processes an init script file.
 *
 * We look for lines that start with the string "FILE" to indicate that they'll be added into the
 * init bundle.
 */
static bool LoadFiles(InitBundle &bundle) {
    // open init script
    std::ifstream scriptFile(gScriptPath);
    if(scriptFile.fail()) return false;

    // read it line by line...
    for(std::string _line; std::getline(scriptFile, _line);) {
        // trim whitespace
        auto line = trim(_line);

        // ignore comments
        if(line.find("#") == 0) continue;

        // handle file lines
        if(line.find("FILE") == 0) {
            // extract the filename
            auto off = line.find_first_of(' ');
            if(off == std::string::npos) {
                std::cerr << "Invalid line: '" << line << "'" << std::endl;
                return false;
            }

            const auto file = line.substr(off+1);
            bundle.addFile(file);
        }
    }

    // if we get here, we've read the entire file
    return true;
}

/**
 * Entry point for the init bundle creator.
 *
 * This will read in an init script, which specifies the files that should be contained in the
 * bundle. Optionally, a syroot may be specified that's prepended to all paths inside the init
 * script.
 */
int main(int argc, char * const *argv) {
    if(!ParseCommandline(argc, argv)) {
        std::cerr << "usage: " << argv[0] << " [-s sysroot] -i script -o outfile" << std::endl;
        return -1;
    }

    if(!gSysroot.empty()) {
        std::filesystem::path p(gSysroot);
        p = std::filesystem::absolute(p);

        gSysroot = p.string();
        std::cout << "Using sysroot: " << gSysroot << std::endl;
    }

    // build up the file container
    InitBundle bundle(gSysroot);

    if(!LoadFiles(bundle)) {
        std::cerr << "failed to read init script" << std::endl;
        return 1;
    }

    // write out the bundle
    const auto written = bundle.write(gOutPath);

    std::cout << "Wrote " << written << " bytes (" << bundle.getNumFiles() << " files in bundle)"
        << std::endl;

    // done!
    return 0;
}
