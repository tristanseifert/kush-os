###############################################################################
# x86 (32-bit Intel) architecture support
###############################################################################
###############################################################################
# build the userspace syscall stub
add_executable(kernel_arch_x86_syscall_stub
    src/syscall/userstub.S
)

set(ARCH_SYSCALL_STUB_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/syscall_stub.ld CACHE STRING "x86 syscall stub linker script" FORCE)
set_target_properties(kernel_arch_x86_syscall_stub PROPERTIES LINK_DEPENDS ${ARCH_SYSCALL_STUB_LINKER_SCRIPT})
target_link_options(kernel_arch_x86_syscall_stub PUBLIC "-T${ARCH_SYSCALL_STUB_LINKER_SCRIPT}")

set_target_properties(kernel_arch_x86_syscall_stub PROPERTIES OUTPUT_NAME "x86_syscall_stub")
set_target_properties(kernel_arch_x86_syscall_stub PROPERTIES SUFFIX "${PLATFORM_KERNEL_EXTENSION}")

# extract the .text section as a flat binary
add_custom_command(TARGET kernel_arch_x86_syscall_stub POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ARGS -O binary --only-section=.text ${CMAKE_CURRENT_BINARY_DIR}/x86_syscall_stub${PLATFORM_KERNEL_EXTENSION} ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub
)
# then convert it to an object file we can link
add_custom_command(TARGET kernel_arch_x86_syscall_stub POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ARGS -I binary -O elf32-i386 syscall_stub ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub.o
)

###############################################################################
# build the actual arch code
add_library(kernel_arch_x86 OBJECT
    src/init.cpp
    src/exceptions.cpp
    src/exceptions.S
    src/gdt.cpp
    src/gdt.S
    src/idt.cpp
    src/vm/PTEHandler.cpp
    src/vm/PDPTPool.cpp
    src/sched/Thread.cpp
    src/sched/switchto.S
    src/syscall/entry.S
    src/syscall/Handler.cpp
    src/math.c
)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub.o PROPERTIES
    EXTERNAL_OBJECT TRUE# object file
    GENERATED TRUE      # doesn't exist at configure time
)

target_link_libraries(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub.o)

add_dependencies(kernel_arch_x86 kernel_arch_x86_syscall_stub)

target_include_directories(kernel_arch_x86 PUBLIC include)
target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_arch_x86 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_include_directories(kernel_arch_x86 PRIVATE src)

set(ARCH_TARGET_NAME "kernel_arch_x86" CACHE STRING "Architecture target name" FORCE)
set(ARCH_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Architecture include directory" FORCE)
