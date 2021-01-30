###############################################################################
# Toolchain descripion for using the system's clang compiler to build for x86
# 32-bit in ELF format.
###############################################################################
SET(CMAKE_SYSTEM_NAME generic)
#SET(CMAKE_SYSTEM_VERSION 1)

SET(CMAKE_ASM_COMPILER clang)
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_ASM_FLAGS "-target i386-none-elf ${CMAKE_ASM_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_C_FLAGS "-target i386-none-elf -ffreestanding ${CMAKE_C_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_CXX_FLAGS "-target i386-none-elf -ffreestanding -fno-exceptions -fno-rtti ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-target i386-linux-elf -nostdlib")

# for macOS, ensure we use the "new and improved(tm)" lld
IF(APPLE)
    SET(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -fuse-ld=lld")
ENDIF()
