###############################################################################
# Toolchain descripion for using the custom toolchain's clang compiler to build
# for x86_64 64-bit in ELF format.
###############################################################################
SET(CMAKE_SYSTEM_NAME kush)
SET(CMAKE_SYSTEM_PROCESSOR "x86_64")

SET(TARGET_TRIPLE "amd64-pc-kush-elf")

# minimum requirement becomes Nehalem-ish; at least SSE4.2, CMPXCHG16B, POPCNT
SET(ARCH_FLAGS "-march=x86-64-v2")

SET(TOOLCHAIN_BASE ${CMAKE_CURRENT_LIST_DIR}/../toolchain/llvm)
get_filename_component(TOOLCHAIN_BASE ${TOOLCHAIN_BASE} ABSOLUTE)
SET(TOOLS_BASE "${TOOLCHAIN_BASE}/bin")

SET(CMAKE_SYSROOT ${CMAKE_CURRENT_LIST_DIR}/../sysroot)
get_filename_component(CMAKE_SYSROOT ${CMAKE_SYSROOT} ABSOLUTE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../cmake/)

# set up search paths for tools
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_BASE})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# specify the binaries
SET(CMAKE_ASM_COMPILER "${TOOLS_BASE}/clang")
SET(CMAKE_C_COMPILER "${TOOLS_BASE}/clang")
SET(CMAKE_CXX_COMPILER "${TOOLS_BASE}/clang++")
SET(CMAKE_AR "${TOOLS_BASE}/llvm-ar")
SET(CMAKE_RANLIB "${TOOLS_BASE}/llvm-ranlib")

# use the host's NASM
SET(CMAKE_ASM_NASM_COMPILER "nasm")

set(CMAKE_ASM_NASM_FLAGS "" FORCE)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -o <OBJECT> <SOURCE>")

# default arguments
SET(CMAKE_ASM_FLAGS "${ASM_FLAGS} -target ${TARGET_TRIPLE} ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_C_FLAGS "${C_CXX_FLAGS} -target ${TARGET_TRIPLE} ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_CXX_FLAGS "${C_CXX_FLAGS} -target ${TARGET_TRIPLE} ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# skip testing compilers
set(CMAKE_ASM_COMPILER_WORKS 1)
set(CMAKE_ASM_NASM_COMPILER_WORKS 1)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# use the default rpath
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "/System/Libraries;/Local/Libraries")
