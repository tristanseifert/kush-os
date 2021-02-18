###############################################################################
# Toolchain descripion for using the system's clang compiler to build for x86
# 32-bit in ELF format.
###############################################################################
SET(CMAKE_SYSTEM_NAME generic)
#SET(CMAKE_SYSTEM_VERSION 1)

SET(TARGET_TRIPLE "i386-pc-kush-elf")
SET(ARCH_FLAGS "-march=pentium3")

SET(TOOLCHAIN_BASE ~/Tools/toolchain/llvm)
get_filename_component(TOOLCHAIN_BASE ${TOOLCHAIN_BASE} ABSOLUTE)
SET(TOOLS_BASE "${TOOLCHAIN_BASE}/bin")

#SET(CMAKE_SYSROOT ${TOOLCHAIN_BASE})

# set up search paths for tools
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_BASE})
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# specify the binaries
SET(CMAKE_ASM_COMPILER "${TOOLS_BASE}/clang")
SET(CMAKE_C_COMPILER "${TOOLS_BASE}/clang")
SET(CMAKE_CXX_COMPILER "${TOOLS_BASE}/clang++")
#SET(CMAKE_LINKER "${TOOLS_BASE}/ld.lld")
SET(CMAKE_AR "${TOOLS_BASE}/llvm-ar")
SET(CMAKE_RANLIB "${TOOLS_BASE}/llvm-ranlib")

# default arguments
SET(CMAKE_ASM_FLAGS "-target ${TARGET_TRIPLE} ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_C_FLAGS "${C_CXX_FLAGS} -target ${TARGET_TRIPLE} ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_CXX_FLAGS "${C_CXX_FLAGS} -target ${TARGET_TRIPLE} ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# skip testing compilers
set(CMAKE_ASM_COMPILER_WORKS 1)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

