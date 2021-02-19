#ifndef _MKINIT_INITBUNDLE_H
#define _MKINIT_INITBUNDLE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * Builds an in-memory list of all files to go into an init bundle, then reads them all in,
 * compresses them, and writes them out.
 */
class InitBundle {
    public:
        InitBundle() = default;
        InitBundle(const std::string &_sysroot) : sysroot(_sysroot) {}

        void addFile(const std::string &inPath);
        size_t write(const std::string &path);

        /// Returns the total number of files
        const size_t getNumFiles() const {
            return this->files.size();
        }

    private:
        /// Info on a file to be contained in the init bundle
        struct File {
            /// filesystem path to the file
            std::string path;
            /// name to be inserted in the header of the bundle
            std::string name;

            /// number of bytes of file data (uncompressed)
            size_t rawBytes;
            /// compressed file data
            std::vector<uint8_t> data;

            File(const std::string &_name, const std::string &_path) : path(_path), name(_name) {}
        };

    private:
        /// path to prepend to filenames when reading from fs
        std::string sysroot;

        /// all files in the bundle
        std::vector<File> files;
};

#endif
