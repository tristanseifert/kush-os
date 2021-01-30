###############################################################################
# Toolchain descripion for using the system's clang compiler to build for x86
# 32-bit in ELF format.
###############################################################################
SET(CMAKE_SYSTEM_NAME generic)
#SET(CMAKE_SYSTEM_VERSION 1)

SET(TARGET_TRIPLE "i386-pc-elf")

SET(CMAKE_ASM_COMPILER clang)
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_ASM_FLAGS "-target ${TARGET_TRIPLE}" CACHE STRING "" FORCE)
SET(CMAKE_C_FLAGS "-target ${TARGET_TRIPLE} -ffreestanding" CACHE STRING "" FORCE)
SET(CMAKE_CXX_FLAGS "-target ${TARGET_TRIPLE} -ffreestanding -fno-exceptions -fno-rtti" CACHE STRING "" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-target ${TARGET_TRIPLE} -nostdlib")

# for macOS, ensure we use the "new and improved(tm)" lld
IF(APPLE)
    #    SET(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fuse-ld=lld")
SET(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fuse-ld=/opt/local/bin/i386-elf-ld")
ENDIF()
