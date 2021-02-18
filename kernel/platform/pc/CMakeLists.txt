###############################################################################
# Platform configuration for x86/x86_64 IBM PC compatibles.
###############################################################################

# platform specific code
add_library(kernel_platform_pc OBJECT
    src/boot.S
    src/panic.S
    src/platform.cpp
    src/multiboot.cpp
    src/physmap.cpp
    src/InitTask.cpp
    src/acpi/Manager.cpp
    src/io/spew.cpp
    src/irq/handlers.S
    src/irq/Manager.cpp
    src/irq/pic.cpp
    src/irq/IoApic.cpp
    src/irq/Apic.cpp
    src/irq/IsrDispatcher.cpp
    src/timer/Manager.cpp
    src/timer/pit.cpp
    src/timer/LocalApicTimer.cpp
)

target_include_directories(kernel_platform_pc PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_include_directories(kernel_platform_pc PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_platform_pc PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

target_include_directories(kernel_platform_pc PRIVATE ${ARCH_INCLUDE_DIR})

target_compile_options(kernel_platform_pc PRIVATE ${KERNEL_COMPILE_OPTS})

set(PLATFORM_TARGET_NAME "kernel_platform_pc" CACHE STRING "Platform target name" FORCE)
set(PLATFORM_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Platform include directory" FORCE)

# Define linker options
set(PLATFORM_KERNEL_EXTENSION ".elf" CACHE STRING "File extension for kernel binary")
set(PLATFORM_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker.ld CACHE FILEPATH "Platform specific linker script")
