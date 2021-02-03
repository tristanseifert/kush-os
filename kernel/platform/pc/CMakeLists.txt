###############################################################################
# Platform configuration for x86/x86_64 IBM PC compatibles.
###############################################################################

# platform specific code
add_library(kernel_platform_pc OBJECT
    src/boot.S
    src/panic.S
    src/platform.c
    src/multiboot.c
    src/physmap.c
    src/io/spew.c
    src/irq/pic.c
)

target_include_directories(kernel_platform_pc PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)

target_include_directories(kernel_platform_pc PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_platform_pc PRIVATE ${ARCH_INCLUDE_DIR})

set(PLATFORM_TARGET_NAME "kernel_platform_pc" CACHE STRING "Platform target name" FORCE)
set(PLATFORM_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Platform include directory" FORCE)

# Define linker options
set(PLATFORM_KERNEL_EXTENSION ".elf" CACHE STRING "File extension for kernel binary")
set(PLATFORM_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker.ld CACHE FILEPATH "Platform specific linker script")
