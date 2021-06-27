###############################################################################
# System interface library
#
# Provides low level helpers, syscall wrappers, and basic implementations of
# some userspace stuff.
###############################################################################

add_library(system_objs OBJECT
    src/sys/port.c
    src/sys/notification.c
    src/sys/irq.c
    src/sys/scheduler.c
    src/sys/task.c
    src/sys/vm.c
    src/sys/infopage.c
    # stack stuff
    src/stack/stack_guard.c
    src/helpers.c
)
target_compile_options(system_objs PRIVATE -fPIC)

# allow the library to have link time optimization
target_compile_options(system_objs PRIVATE -flto -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables)
# set required compiler flags
target_compile_definitions(system_objs PRIVATE BUILDING_LIBSYSTEM=1)
# specify the include directories
target_include_directories(system_objs PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(system_objs PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# set up the static and dynamic targets
add_library(system_static STATIC $<TARGET_OBJECTS:system_objs>)
set_target_properties(system_static PROPERTIES OUTPUT_NAME "system")
target_include_directories(system_static PUBLIC include)

add_library(system_dynamic SHARED $<TARGET_OBJECTS:system_objs>)
set_target_properties(system_dynamic PROPERTIES OUTPUT_NAME "system")
target_include_directories(system_dynamic PUBLIC include)

# build the correct platform-specific code
if(${KERNEL_ARCH} STREQUAL "x86")
    target_sources(system_objs PRIVATE
        src/x86/threads.c
        src/x86/thread_start.S
        src/x86/syscall_arch.c
        src/x86/syscall_impl.c
    )

    target_compile_definitions(system_objs PRIVATE LIBC_ARCH_X86=1)

    # ensure the x86 headers make it
    FILE(GLOB LIBSYSTEM_PUB_SYS_X86_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/x86/*.h")
    install(FILES ${LIBSYSTEM_PUB_SYS_X86_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys/x86)
elseif(${KERNEL_ARCH} STREQUAL "x86_64")
    target_sources(system_objs PRIVATE
        src/x86_64/threads.c
        src/x86_64/thread_start.S
        src/x86_64/syscall_arch.c
        src/x86_64/syscall_impl.S
        src/x86_64/backtrace.c
    )

    target_compile_definitions(system_objs PRIVATE LIBC_ARCH_X86_64=1)

    # ensure the x86_64 headers make it
    FILE(GLOB LIBSYSTEM_PUB_SYS_X64_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/amd64/*.h")
    install(FILES ${LIBSYSTEM_PUB_SYS_X64_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys/amd64)
else()
    message(FATAL_ERROR "libsystem does not know how architecture '${KERNEL_ARCH}'")
endif()

# install the library to the sysroot
install(TARGETS system_dynamic LIBRARY)
add_library(system ALIAS system_dynamic)

add_custom_command(TARGET system_dynamic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libsystem.so ${SYSROOT_DIR}/boot/lib/libsystem.so)

FILE(GLOB LIBSYSTEM_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBSYSTEM_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include)
FILE(GLOB LIBSYSTEM_PUB_SYS_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/*.h*")
install(FILES ${LIBSYSTEM_PUB_SYS_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys)

# copy the rpc headers
FILE(GLOB LIBSYSTEM_RPC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rpc/*.hpp")
install(FILES ${LIBSYSTEM_RPC_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/rpc)

