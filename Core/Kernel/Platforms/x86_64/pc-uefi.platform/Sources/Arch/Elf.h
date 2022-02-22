/**
 * ELF data structures
 *
 * This has only 64-bit ELF structures.
 */
#ifndef KERNEL_PLATFORM_UEFI_ARCH_ELF
#define KERNEL_PLATFORM_UEFI_ARCH_ELF

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
 * 64-bit ELF section header
 */
typedef struct {
    uint32_t	sh_name;	/* Section name (index into the
                                       section header string table). */
    uint32_t	sh_type;	/* Section type. */
    uint64_t	sh_flags;	/* Section flags. */
    uint64_t	sh_addr;	/* Address in memory image. */
    uint64_t	sh_offset;	/* Offset in file. */
    uint64_t	sh_size;	/* Size in bytes. */
    uint32_t	sh_link;	/* Index of a related section. */
    uint32_t	sh_info;	/* Depends on section type. */
    uint64_t	sh_addralign;	/* Alignment in bytes. */
    uint64_t	sh_entsize;	/* Size of each entry in section. */
} __attribute__((packed)) Elf64_Shdr;

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

/*
 * Symbol table entries.
 */

typedef struct {
    uint32_t	st_name;	/* String table index of name. */
    uint8_t	st_info;	/* Type and binding information. */
    uint8_t	st_other;	/* Reserved (not used). */
    uint16_t	st_shndx;	/* Section index of symbol. */
    uint64_t	st_value;	/* Symbol value. */
    uint64_t	st_size;	/* Size of associated object. */
} __attribute__((packed)) Elf64_Sym;

/* Macros for accessing the fields of st_info. */
#define	ELF64_ST_BIND(info)		((info) >> 4)
#define	ELF64_ST_TYPE(info)		((info) & 0xf)

/* Macro for constructing st_info from field values. */
#define	ELF64_ST_INFO(bind, type)	(((bind) << 4) + ((type) & 0xf))

/* Macro for accessing the fields of st_other. */
#define	ELF64_ST_VISIBILITY(oth)	((oth) & 0x3)

/* Indexes into the e_ident array.  Keep synced with
   http://www.sco.com/developers/gabi/latest/ch4.eheader.html */
#define	EI_MAG0		0	/* Magic number, byte 0. */
#define	EI_MAG1		1	/* Magic number, byte 1. */
#define	EI_MAG2		2	/* Magic number, byte 2. */
#define	EI_MAG3		3	/* Magic number, byte 3. */
#define	EI_CLASS	4	/* Class of machine. */
#define	EI_DATA		5	/* Data format. */
#define	EI_VERSION	6	/* ELF format version. */
#define	EI_OSABI	7	/* Operating system / ABI identification */
#define	EI_ABIVERSION	8	/* ABI version */
#define	OLD_EI_BRAND	8	/* Start of architecture identification. */
#define	EI_PAD		9	/* Start of padding (per SVR4 ABI). */
#define	EI_NIDENT	16	/* Size of e_ident array. */

/* Values for the magic number bytes. */
#define	ELFMAG0		0x7f
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"	/* magic string */
#define	SELFMAG		4		/* magic string size */

/* Values for e_ident[EI_VERSION] and e_version. */
#define	EV_NONE		0
#define	EV_CURRENT	1

/* Values for e_ident[EI_CLASS]. */
#define	ELFCLASSNONE	0	/* Unknown class. */
#define	ELFCLASS32	1	/* 32-bit architecture. */
#define	ELFCLASS64	2	/* 64-bit architecture. */

/* Values for e_ident[EI_DATA]. */
#define	ELFDATANONE	0	/* Unknown data format. */
#define	ELFDATA2LSB	1	/* 2's complement little-endian. */
#define	ELFDATA2MSB	2	/* 2's complement big-endian. */

/* Values for e_ident[EI_OSABI]. */
#define	ELFOSABI_NONE		0	/* UNIX System V ABI */
#define	ELFOSABI_HPUX		1	/* HP-UX operating system */
#define	ELFOSABI_NETBSD		2	/* NetBSD */
#define	ELFOSABI_LINUX		3	/* GNU/Linux */
#define	ELFOSABI_HURD		4	/* GNU/Hurd */
#define	ELFOSABI_86OPEN		5	/* 86Open common IA32 ABI */
#define	ELFOSABI_SOLARIS	6	/* Solaris */
#define	ELFOSABI_AIX		7	/* AIX */
#define	ELFOSABI_IRIX		8	/* IRIX */
#define	ELFOSABI_FREEBSD	9	/* FreeBSD */
#define	ELFOSABI_TRU64		10	/* TRU64 UNIX */
#define	ELFOSABI_MODESTO	11	/* Novell Modesto */
#define	ELFOSABI_OPENBSD	12	/* OpenBSD */
#define	ELFOSABI_OPENVMS	13	/* Open VMS */
#define	ELFOSABI_NSK		14	/* HP Non-Stop Kernel */
#define	ELFOSABI_AROS		15	/* Amiga Research OS */
#define	ELFOSABI_FENIXOS	16	/* FenixOS */
#define	ELFOSABI_CLOUDABI	17	/* Nuxi CloudABI */
#define	ELFOSABI_OPENVOS	18	/* Stratus Technologies OpenVOS */
#define	ELFOSABI_ARM_AEABI	64	/* ARM EABI */
#define	ELFOSABI_ARM		97	/* ARM */
#define	ELFOSABI_STANDALONE	255	/* Standalone (embedded) application */

#define	ELFOSABI_SYSV		ELFOSABI_NONE	/* symbol used in old spec */
#define	ELFOSABI_MONTEREY	ELFOSABI_AIX	/* Monterey */
#define	ELFOSABI_GNU		ELFOSABI_LINUX

/* e_ident */
#define	IS_ELF(ehdr)	((ehdr).ident[EI_MAG0] == ELFMAG0 && \
			 (ehdr).ident[EI_MAG1] == ELFMAG1 && \
			 (ehdr).ident[EI_MAG2] == ELFMAG2 && \
			 (ehdr).ident[EI_MAG3] == ELFMAG3)

/* Special section indexes. */
#define	SHN_UNDEF	     0		/* Undefined, missing, irrelevant. */

/* sh_type */
#define	SHT_SYMTAB		2	/* symbol table section */
#define	SHT_STRTAB		3	/* string table section */
#define	SHT_RELA		4	/* relocation section with addends */
#define	SHT_HASH		5	/* symbol hash table section */
#define	SHT_DYNAMIC		6	/* dynamic section */
#define	SHT_NOTE		7	/* note section */
#define	SHT_REL			9	/* relocation section - no addends */
#define	SHT_DYNSYM		11	/* dynamic symbol table section */
#define	SHT_SYMTAB_SHNDX	18	/* Section indexes (see SHN_XINDEX). */

/* Values for p_type. */
#define	PT_NULL		0	/* Unused entry. */
#define	PT_LOAD		1	/* Loadable segment. */
#define	PT_DYNAMIC	2	/* Dynamic linking information segment. */
#define	PT_INTERP	3	/* Pathname of interpreter. */
#define	PT_NOTE		4	/* Auxiliary information. */
#define	PT_SHLIB	5	/* Reserved (not used). */
#define	PT_PHDR		6	/* Location of program header itself. */
#define	PT_TLS		7	/* Thread local storage segment */

/* Values for p_flags. */
#define	PF_X		0x1		/* Executable. */
#define	PF_W		0x2		/* Writable. */
#define	PF_R		0x4		/* Readable. */

/* Symbol Binding - ELFNN_ST_BIND - st_info */
#define	STB_LOCAL	0	/* Local symbol */
#define	STB_GLOBAL	1	/* Global symbol */
#define	STB_WEAK	2	/* like global - lower precedence */
#define	STB_LOOS	10	/* Start of operating system reserved range. */
#define	STB_GNU_UNIQUE	10	/* Unique symbol (GNU) */
#define	STB_HIOS	12	/* End of operating system reserved range. */
#define	STB_LOPROC	13	/* reserved range for processor */
#define	STB_HIPROC	15	/*   specific semantics. */

/* Symbol type - ELFNN_ST_TYPE - st_info */
#define	STT_NOTYPE	0	/* Unspecified type. */
#define	STT_OBJECT	1	/* Data object. */
#define	STT_FUNC	2	/* Function. */
#define	STT_SECTION	3	/* Section. */
#define	STT_FILE	4	/* Source file. */
#define	STT_COMMON	5	/* Uninitialized common block. */
#define	STT_TLS		6	/* TLS object. */
#define	STT_NUM		7
#define	STT_LOOS	10	/* Reserved range for operating system */
#define	STT_GNU_IFUNC	10
#define	STT_HIOS	12	/*   specific semantics. */
#define	STT_LOPROC	13	/* Start of processor reserved range. */
#define	STT_SPARC_REGISTER 13	/* SPARC register information. */
#define	STT_HIPROC	15	/* End of processor reserved range. */

/* Symbol visibility - ELFNN_ST_VISIBILITY - st_other */
#define	STV_DEFAULT	0x0	/* Default visibility (see binding). */
#define	STV_INTERNAL	0x1	/* Special meaning in relocatable objects. */
#define	STV_HIDDEN	0x2	/* Not visible. */
#define	STV_PROTECTED	0x3	/* Visible but not preemptible. */
#define	STV_EXPORTED	0x4
#define	STV_SINGLETON	0x5
#define	STV_ELIMINATE	0x6

/* Special symbol table indexes. */
#define	STN_UNDEF	0	/* Undefined symbol index. */

#endif
