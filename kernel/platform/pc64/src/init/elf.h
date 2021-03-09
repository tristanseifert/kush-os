#ifndef PLATFORM_PC64_ELF_H
#define PLATFORM_PC64_ELF_H

#include <stdint.h>

#define EI_NIDENT 16

/**
 * 64-bit ELF fixed file header
 */
typedef struct {
    unsigned char ident[16];
    // ELF file type and CPU arch
    uint16_t type, machine;
    // version (should be 1)
    uint32_t version;

    // virtual address of entry point
    uint64_t entryAddr;

    // file relative offset to program headers
    uint64_t progHdrOff;
    // file relative offset to section headers
    uint64_t secHdrOff;

    // machine specific flags
    uint32_t flags;

    // size of this header
    uint16_t headerSize;
    // size of a program header
    uint16_t progHdrSize;
    // number of program headers
    uint16_t numProgHdr;
    // size of a section header
    uint16_t secHdrSize;
    // number of section headers
    uint16_t numSecHdr;

    // section header index for the string table
    uint16_t stringSectionIndex;
} __attribute__((packed)) Elf64_Ehdr;

/**
 * 64-bit ELF program header
 */
typedef struct {
    // type of this header
    uint32_t type;
    // flags
    uint32_t flags;
    // file offset to the first byte of this segment
    uint64_t fileOff;

    // virtual address of this mapping
    uint64_t virtAddr;
    // physical address of this mapping (ignored)
    uint64_t physAddr;

    // number of bytes in the file image for this segment
    uint64_t fileBytes;
    // number of bytes of memory to use
    uint64_t memBytes;

    // alignment flags
    uint64_t align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_NULL                         0
#define PT_LOAD                         1
#define PT_PHDR                         6

#define PF_EXECUTABLE                   (1 << 0)
#define PF_WRITE                        (1 << 1)
#define PF_READ                         (1 << 2)

#endif
