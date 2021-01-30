###############################################################################
# Platform configuration for x86/x86_64 IBM PC compatibles.
###############################################################################

# platform specific code
add_library(kernel_platform_pc OBJECT
    ${CMAKE_CURRENT_LIST_DIR}/src/boot.S
    ${CMAKE_CURRENT_LIST_DIR}/src/platform.c
    ${CMAKE_CURRENT_LIST_DIR}/src/irq/pic.c
)

target_include_directories(kernel_platform_pc PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_platform_pc PRIVATE ${ARCH_INCLUDE_DIR})

set(PLATFORM_TARGET_NAME "kernel_platform_pc" CACHE STRING "Platform target name" FORCE)

# Define linker options
set(PLATFORM_KERNEL_EXTENSION ".elf" CACHE STRING "File extension for kernel binary")
set(PLATFORM_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker.ld CACHE FILEPATH "Platform specific linker script")
