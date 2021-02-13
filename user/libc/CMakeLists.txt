###############################################################################
# Userspace C library
###############################################################################

add_library(c STATIC
    # string functions
    src/string/memchr.c
    src/string/memclr.c
    src/string/memcmp.c
    src/string/memcpy.c
    src/string/memmove.c
    src/string/memset.c
    src/string/printf.c
    src/string/strlen.c
    src/string/strncat.c
    src/string/strncmp.c
    src/string/strncpy.c
    src/string/strnlen.c
    src/string/strnstr.c
    src/sys/abort.c
    src/sys/scheduler.c
    src/sys/task.c
    src/stack/stack_guard.c
)

target_compile_definitions(c PRIVATE BUILDING_LIBC=1)

target_include_directories(c PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(c PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# build the correct platform-specific code
if(${KERNEL_ARCH} STREQUAL "x86")
    target_sources(c PRIVATE
        src/x86/syscall_impl.c
        src/x86/start.S
        src/x86/div64.c
    )

    # export settings to build statically linked executables
    set(C_STATIC_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/src/x86/app_static.ld CACHE FILEPATH "Statically linked C executable linker script")
    set(C_STATIC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/libc.a)
else()
    message(FATAL_ERROR "libc does not know how architecture '${KERNEL_ARCH}'")
endif()
