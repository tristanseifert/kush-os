#ifndef _MKINIT_BUNDLETYPES_H
#define _MKINIT_BUNDLETYPES_H

#include <stddef.h>
#include <stdint.h>

/**
 * Header describing a single file in the init bundke.
 */
struct InitFileHeader {
    /// flags: 0x80000000 = compressed
    uint32_t flags;

    /// file offset (0 = start of bundle) to the file's data
    uint32_t dataOff;
    /// number of bytes stored in init file
    uint32_t dataLen;
    /// total size of the file, in bytes (maybe different from `dataLen` if compressed)
    uint32_t rawLen;

    /// length of the filename field
    uint8_t nameLen;
    /// name bytes
    char name[];
} __attribute__((packed));

/**
 * Header of an init bundle
 */
struct InitHeader {
    /// magic value: must be 'KUSH'
    uint32_t magic;
    /// major and minor version: must be 1,0 respectively
    uint16_t major, minor;
    /// bundle type: must be 'INIT'
    uint32_t type;

    /// total length of header
    uint32_t headerLen;
    /// total length of bundle, including payload and padding
    uint32_t totalLen;

    /// number of file entries
    uint32_t numFiles;
    /// file headers
    struct InitFileHeader headers[];
} __attribute__((packed));

constexpr static const uint32_t kInitMagic = 'HSUK';
constexpr static const uint32_t kInitType = 'TINI';

#endif
