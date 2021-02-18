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
    # syscalls
    src/sys/abort.c
    src/sys/assert.c
    src/sys/port.c
    src/sys/scheduler.c
    src/sys/task.c
    src/sys/vm.c
    # stdlib math stuff
    src/stdlib/div.c
    src/stdlib/labs.c
    src/stdlib/ldiv.c
    src/stdlib/llabs.c
    src/stdlib/lldiv.c
    # locale
    src/locale/isctype.c
    # stack stuff
    src/stack/stack_guard.c
)

# allow the C library to have link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(c PRIVATE "-flto")
endif()

# set required compiler flags
target_compile_definitions(c PRIVATE BUILDING_LIBC=1)

# specify the include directories
target_include_directories(c PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(c PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# build the correct platform-specific code
if(${KERNEL_ARCH} STREQUAL "x86")
    target_sources(c PRIVATE
        src/x86/crt0.S
        src/x86/crti.c
        src/x86/div64.c
        src/x86/threads.c
        src/x86/thread_start.S
        src/x86/syscall_arch.c
        src/x86/syscall_impl.c
    )

    target_compile_definitions(c PRIVATE LIBC_ARCH_X86=1)

    # export settings to build statically linked executables
    set(C_STATIC_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/src/x86/app_static.ld CACHE FILEPATH "Statically linked C executable linker script")
    set(C_STATIC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/libc.a)

    # ensure the x86 headers make it
    FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/x86/*.h")
    install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys/x86)
else()
    message(FATAL_ERROR "libc does not know how architecture '${KERNEL_ARCH}'")
endif()

# install the library to the sysroot
install(TARGETS c ARCHIVE DESTINATION ${SYSROOT_DIR}/lib)
install(TARGETS c LIBRARY DESTINATION ${SYSROOT_DIR}/lib)

FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include)
FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/*.h")
install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys)

