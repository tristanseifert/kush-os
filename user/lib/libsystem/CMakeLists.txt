###############################################################################
# System interface library
#
# Provides low level helpers, syscall wrappers, and basic implementations of
# some userspace stuff.
###############################################################################

add_library(system_objs OBJECT
    src/sys/port.c
    src/sys/scheduler.c
    src/sys/task.c
    src/sys/vm.c
    # stack stuff
    src/stack/stack_guard.c
)
target_compile_options(system_objs PRIVATE -fPIC)

# allow the library to have link time optimization
target_compile_options(system_objs PRIVATE -flto)
target_compile_options(system_objs PRIVATE -fno-exceptions -fno-rtti)
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

# Link against libunwind.
# This is required for building pretty backtraces, and also has the benefit that it'll make sure
# that C++ exceptions will always work.
target_link_libraries(system_static PUBLIC unwind)
target_link_libraries(system_dynamic PUBLIC unwind)

# build the correct platform-specific code
if(${KERNEL_ARCH} STREQUAL "x86")
    target_sources(system_objs PRIVATE
        src/x86/threads.c
        src/x86/thread_start.S
        src/x86/syscall_arch.c
        src/x86/syscall_impl.c
    )

    target_compile_definitions(system_objs PRIVATE LIBC_ARCH_X86=1)

    # export settings to build statically linked executables
    set(C_STATIC_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/src/x86/app_static.ld CACHE FILEPATH "Statically linked C executable linker script")

    # ensure the x86 headers make it
    FILE(GLOB LIBSYSTEM_PUB_SYS_X86_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/x86/*.h")
    install(FILES ${LIBSYSTEM_PUB_SYS_X86_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys/x86)
else()
    message(FATAL_ERROR "libsystem does not know how architecture '${KERNEL_ARCH}'")
endif()

# install the library to the sysroot
install(TARGETS system_static LIBRARY)
install(TARGETS system_dynamic LIBRARY)

FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include)
FILE(GLOB LIBC_PUB_SYS_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/*.h")
install(FILES ${LIBC_PUB_SYS_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys)
