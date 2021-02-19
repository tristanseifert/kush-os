#include "InitBundle.h"
#include "BundleTypes.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <lzfse.h>

/**
 * Adds a new file to the init bundle.
 *
 * We'll load the file and compress it at this point.
 */
void InitBundle::addFile(const std::string &_path) {
    size_t err;

    // open for reading
    const auto path = std::filesystem::path(_path);
    const auto realpath = (this->sysroot.empty() ? path :
            (std::filesystem::path(this->sysroot + _path)));

    std::ifstream file(realpath, std::istream::binary);
    if(file.fail()) {
        throw std::runtime_error("failed to open input file: " + realpath.string());
    }

    // prepare the file struct
    File hdr(_path, realpath.string());
    hdr.rawBytes = std::filesystem::file_size(realpath);

    // ignore zero byte files
    if(!hdr.rawBytes) {
        std::cerr << "ignoring zero-byte file at " << realpath.string() << std::endl;
    }

    // read the file
    hdr.data.resize(hdr.rawBytes);
    std::vector<uint8_t> contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // compress it
    err = lzfse_encode_buffer(hdr.data.data(), hdr.data.size(), contents.data(), contents.size(), 
            nullptr);
    if(!err) {
        throw std::runtime_error("lzfse_encode_buffer failed");
    }

    hdr.data.resize(err);

    // store the file data
    this->files.push_back(std::move(hdr));
}

/**
 * Builds up the bundle's header, as well as the individual file headers. Then, append the data for
 * each file to the init bundle.
 */
size_t InitBundle::write(const std::string &path) {
    // open up write stream
    std::fstream file(path, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if(file.fail()) {
        throw std::runtime_error("failed to open output file");
    }

    // build the header
    InitHeader hdr;
    memset(&hdr, 0, sizeof(InitHeader));

    hdr.magic = kInitMagic;
    hdr.type = kInitType;
    hdr.major = 1; hdr.minor = 0;
    hdr.numFiles = this->files.size();

    // ensure file headers are sorted
    std::sort(this->files.begin(), this->files.end(), [](const auto &f1, const auto &f2) {
        return f1.name < f2.name;
    });

    // build headers for all files. this will also place them
    void *fileHdrs = nullptr;
    size_t fileHdrOff = 0;
    size_t fileDataOff = 0;

    for(const auto &file : this->files) {
        // calculate space required for this header and allocate it
        const auto required = sizeof(InitFileHeader) + file.name.length();
        fileHdrs = realloc(fileHdrs, fileHdrOff + required);
        if(!fileHdrs) {
            throw std::runtime_error("failed to realloc file headers");
        }

        // populate the file header
        auto hdr = reinterpret_cast<InitFileHeader *>(((uintptr_t) fileHdrs) + fileHdrOff);
        memset(hdr, 0, required);

        hdr->flags = kInitFileFlagsCompressed;
        hdr->dataLen = file.data.size();
        hdr->rawLen = file.rawBytes;
        hdr->dataOff = fileDataOff;
        hdr->nameLen = file.name.length();
        strncpy(hdr->name, file.name.c_str(), hdr->nameLen);

        // prepare for next
        fileDataOff += hdr->dataLen;
        if(fileDataOff % 16) {
            fileDataOff = ((fileDataOff + 15) / 16) * 16;
        }

        fileHdrOff += required;
    }

    // adjust file headers offset values
    const auto hdrBytes = sizeof(InitHeader) + fileHdrOff;
    const auto fileDataStart = ((hdrBytes + 15) / 16) * 16;
    size_t totalSize = 0;

    InitFileHeader *fileHdr = reinterpret_cast<InitFileHeader *>(fileHdrs);
    for(size_t i = 0; i < this->files.size(); i++) {
        fileHdr->dataOff += fileDataStart;

        const auto end = fileHdr->dataOff + fileHdr->dataLen;
        if(end > totalSize) {
            totalSize = end;
        }

        // go to the next header
        fileHdr = reinterpret_cast<InitFileHeader *>(((uintptr_t) fileHdr) + sizeof(InitFileHeader) + fileHdr->nameLen);
    }

    std::cout << "starting file data at " << fileDataStart << std::endl;

    hdr.totalLen = totalSize;

    // write out the bundle header and all file headers
    file.write(reinterpret_cast<const char *>(&hdr), sizeof(InitHeader));
    file.write(reinterpret_cast<const char *>(fileHdrs), fileHdrOff);

    // place files; assume we placed headers in the same order as the sorted files array
    fileHdr = reinterpret_cast<InitFileHeader *>(fileHdrs);
    for(size_t i = 0; i < this->files.size(); i++) {
        // get file info and write it
        const auto &info = this->files[i];

        file.seekp(fileHdr->dataOff);
        file.write(reinterpret_cast<const char *>(info.data.data()), info.data.size());

        // go to the next header
        fileHdr = reinterpret_cast<InitFileHeader *>(((uintptr_t) fileHdr) + sizeof(InitFileHeader) + fileHdr->nameLen);
    }

    // done
    free(fileHdrs);
    return file.tellp();
}
