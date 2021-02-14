/*
 * Basic structures representing ELF file components.
 */
#ifndef PLATFORM_PC_ELF_H
#define PLATFORM_PC_ELF_H

#include <stdint.h>

#define EI_NIDENT 16

/**
 * 32-bit ELF fixed file header
 */
typedef struct {
    unsigned char ident[16];
    // ELF file type and CPU arch
    uint16_t type, machine;
    // version (should be 1)
    uint32_t version;

    // virtual address of entry point
    uint32_t entryAddr;

    // file relative offset to program headers
    uint32_t progHdrOff;
    // file relative offset to section headers
    uint32_t secHdrOff;

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
} __attribute__((packed)) Elf32_Ehdr;

/**
 * 32-bit ELF program header
 */
typedef struct {
    // type of this header
    uint32_t type;
    // file offset to the first byte of this segment
    uint32_t fileOff;

    // virtual address of this mapping
    uint32_t virtAddr;
    // physical address of this mapping (ignored)
    uint32_t physAddr;

    // number of bytes in the file image for this segment
    uint32_t fileBytes;
    // number of bytes of memory to use
    uint32_t memBytes;

    // flags
    uint32_t flags;
    // alignment flags
    uint32_t align;
} __attribute__((packed)) Elf32_Phdr;

#define PT_NULL                         0
#define PT_LOAD                         1
#define PT_PHDR                         6

#define PF_EXECUTABLE                   (1 << 0)
#define PF_WRITE                        (1 << 1)
#define PF_READ                         (1 << 2)

#endif
