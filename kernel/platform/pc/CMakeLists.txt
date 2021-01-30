###############################################################################
# Platform configuration for x86/x86_64 IBM PC compatibles.
###############################################################################

# platform specific code

# Define linker options
set(PLATFORM_KERNEL_EXTENSION ".elf" CACHE STRING "File extension for kernel binary")
set(PLATFORM_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker.ld CACHE FILEPATH "Platform specific linker script")
