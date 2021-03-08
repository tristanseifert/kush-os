###############################################################################
# amd64 (64-bit Intel) architecture support
###############################################################################
###############################################################################
# build the userspace syscall stub
add_executable(kernel_arch_amd64_syscall_stub
    src/syscall/userstub.S
)

set(ARCH_amd64_SYSCALL_STUB_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/syscall_stub.ld CACHE STRING "amd64 syscall stub linker script" FORCE)
set_target_properties(kernel_arch_amd64_syscall_stub PROPERTIES LINK_DEPENDS ${ARCH_amd64_SYSCALL_STUB_LINKER_SCRIPT})
target_compile_options(kernel_arch_amd64_syscall_stub PRIVATE -ffreestanding -nostdlib)
target_link_options(kernel_arch_amd64_syscall_stub PUBLIC -ffreestanding -nostdlib -nostartfiles "-Wl,-T${ARCH_amd64_SYSCALL_STUB_LINKER_SCRIPT}")

set_target_properties(kernel_arch_amd64_syscall_stub PROPERTIES OUTPUT_NAME "amd64_syscall_stub")
set_target_properties(kernel_arch_amd64_syscall_stub PROPERTIES SUFFIX "${PLATFORM_KERNEL_EXTENSION}")

# extract the .text section as a flat binary
add_custom_command(TARGET kernel_arch_amd64_syscall_stub POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ARGS -O binary --only-section=.text ${CMAKE_CURRENT_BINARY_DIR}/amd64_syscall_stub${PLATFORM_KERNEL_EXTENSION} ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub
)
# then convert it to an object file we can link (ensure it will be in .rodata rather than rw .data)
add_custom_command(TARGET kernel_arch_amd64_syscall_stub POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ARGS -I binary --rename-section .data=.rodata,alloc,load,readonly,data,contents -O elf64-x86-64 syscall_stub ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub.o
)

###############################################################################
# build the actual arch code
add_library(kernel_arch_amd64 OBJECT
    src/init.cpp
    src/exceptions.cpp
    src/exceptions.S
    src/gdt.cpp
    src/gdt.S
    src/idt.cpp
    src/PerCpuInfo.cpp
    src/vm/PTEHandler.cpp
    src/sched/Thread.cpp
    src/sched/switchto.S
    src/syscall/entry.S
    src/syscall/Handler.cpp
    src/syscall/ThreadLocal.cpp
    #    src/math.c
)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub.o PROPERTIES
    EXTERNAL_OBJECT TRUE# object file
    GENERATED TRUE      # doesn't exist at configure time
)

target_link_libraries(kernel_arch_amd64 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/syscall_stub.o)

add_dependencies(kernel_arch_amd64 kernel_arch_amd64_syscall_stub)

set(ARCH_COMPILE_OPTS -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 CACHE STRING "Architecture specific compile flags")

target_compile_options(kernel_arch_amd64 PRIVATE ${KERNEL_COMPILE_OPTS} ${ARCH_COMPILE_OPTS})

target_include_directories(kernel_arch_amd64 PUBLIC include)
target_include_directories(kernel_arch_amd64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_include_directories(kernel_arch_amd64 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_include_directories(kernel_arch_amd64 PRIVATE src)

set(ARCH_TARGET_NAME "kernel_arch_amd64" CACHE STRING "Architecture target name" FORCE)
set(ARCH_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include CACHE FILEPATH "Architecture include directory" FORCE)

