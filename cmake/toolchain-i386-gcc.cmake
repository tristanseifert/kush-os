###############################################################################
# Toolchain descripion for using a gcc cross toolchain, on the PATH, to compile
# the system
###############################################################################
SET(CMAKE_SYSTEM_NAME generic)
#SET(CMAKE_SYSTEM_VERSION 1)

SET(TARGET_TRIPLE "i386-pc-elf")

# optimize for architecture (P3 min CPU target)
SET(ARCH_FLAGS "-march=pentium3")

SET(CMAKE_ASM_COMPILER i386-elf-gcc)
SET(CMAKE_C_COMPILER i386-elf-gcc)
SET(CMAKE_CXX_COMPILER i386-elf-g++)
SET(CMAKE_ASM_FLAGS "" CACHE STRING "" FORCE)
SET(CMAKE_C_FLAGS "-ffreestanding ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_CXX_FLAGS "-ffreestanding -fno-exceptions -fno-rtti ${ARCH_FLAGS}" CACHE STRING "" FORCE)
SET(CMAKE_EXE_LINKER_FLAGS_INIT "-ffreestanding -nostdlib")
