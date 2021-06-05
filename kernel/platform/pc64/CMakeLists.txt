###############################################################################
# Platform configuration for x86_64 PCs, booted using the BOOTBOOT 64-bit
# native loader.
#
# This means the kernel doesn't contain any sort of 32-bit trampoline: the
# loader places us directly into 64-bit mode.
###############################################################################

# platform specific code
add_library(kernel_platform_pc64 OBJECT
    src/boot.S
    src/panic.S
    src/platform.cpp
    src/physmap.cpp
    src/io/spew.cpp
    src/irq/Manager.cpp
    src/irq/LocalApic.cpp
    src/irq/IoApic.cpp
    src/irq/Irql.cpp
    src/irq/Ipi.cpp
    src/irq/Registry.cpp
    src/irq/pic.cpp
    src/irq/Idle.cpp
    src/timer/ApicTimer.cpp
    src/timer/Hpet.cpp
    src/timer/Tsc.cpp
    src/timer/Registry.cpp
    src/timer/pit.cpp
    src/init/LoadRootsrv.cpp
    src/acpi/Parser.cpp
    src/syscall/LoaderInfo.cpp
)

target_include_directories(kernel_platform_pc64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_include_directories(kernel_platform_pc64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_platform_pc64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(kernel_platform_pc64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

target_include_directories(kernel_platform_pc64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/bootboot/dist)

target_include_directories(kernel_platform_pc64 PRIVATE ${ARCH_INCLUDE_DIR})

target_compile_options(kernel_platform_pc64 PRIVATE ${KERNEL_COMPILE_OPTS} ${ARCH_COMPILE_OPTS})

set(PLATFORM_TARGET_NAME "kernel_platform_pc64" CACHE STRING "Platform target name" FORCE)
set(PLATFORM_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Platform include directory" FORCE)

# Define linker options
set(PLATFORM_KERNEL_EXTENSION ".elf" CACHE STRING "File extension for kernel binary")
set(PLATFORM_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker.ld CACHE FILEPATH "Platform specific linker script")

# install the config file
install(FILES ${CMAKE_CURRENT_LIST_DIR}/kernconf-pc64 DESTINATION ${SYSROOT_DIR}/boot)

# post-install step for the kernel: build bootboot image
set(BOOTBOOT_MKBOOTIMAGE ${CMAKE_CURRENT_LIST_DIR}/bootboot/mkbootimg/mkbootimg)

