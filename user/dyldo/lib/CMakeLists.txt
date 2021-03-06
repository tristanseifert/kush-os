###############################################################################
# Dynamic linker runtime
#
# This is a static executable which the program loader will load into the
# address space of all new tasks (if they have this value in their requested
# interpreter field, of course) and jump to its entry point.
#
# We can't make use of exceptions yet, and all the C++ runtime fancy stuff is
# disabled. Any errors will _always_ be fatal and cause the task to terminate.
###############################################################################
add_executable(dyldo
    src/init/lib_init.cpp
    src/init/CppRuntime.cpp
    src/elf/ElfReader.cpp
    src/elf/ElfReader+IntelRelocs.cpp
    src/elf/ElfExecReader.cpp
    src/elf/ElfLibReader.cpp
    src/link/SymbolMap.cpp
    src/runtime/DlInfo.cpp
    src/runtime/ThreadLocal.cpp
    src/Linker.cpp
)

if(${KERNEL_ARCH} STREQUAL "x86")
    target_sources(dyldo PRIVATE
        src/init/entry_x86.S
        src/init/jmp_to_x86.S
    )

    set_target_properties(dyldo PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/linker_x86.ld)
    target_link_options(dyldo PUBLIC "-Wl,-T${CMAKE_CURRENT_LIST_DIR}/src/linker_x86.ld")
elseif(${KERNEL_ARCH} STREQUAL "x86_64")
    target_sources(dyldo PRIVATE
        src/init/entry_amd64.S
        src/init/jmp_to_amd64.S
    )

    set_target_properties(dyldo PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/linker_amd64.ld)
    target_link_options(dyldo PUBLIC "-Wl,-T${CMAKE_CURRENT_LIST_DIR}/src/linker_amd64.ld")
else()
    message(FATAL_ERROR "dyldo runtime does not know how architecture '${KERNEL_ARCH}'")
endif()

# define how verbose the linker is
option(DYLDO_ALWAYS_VERBOSE "Whether the dynamic linker is always very verbose" OFF)

if(DYLDO_ALWAYS_VERBOSE)
    target_compile_definitions(dyldo PRIVATE DYLDO_VERBOSE)
endif()

# turn off a bunch of C++ features and disable C library startup cruft
target_compile_options(dyldo PRIVATE -static)
target_compile_options(dyldo PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_options(dyldo PRIVATE -nostartfiles -nolibc -nostdlib++ -static -fno-exceptions -fno-use-init-array)
target_link_libraries(dyldo PRIVATE c_static_notls system_static rpc_static)
target_compile_definitions(dyldo PRIVATE BUILDING_LIBDYLDO=1)

# optimize for size
target_compile_options(dyldo PRIVATE -mcmodel=large)
target_link_options(dyldo PRIVATE -s)

# specify the include directories
target_include_directories(dyldo PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(dyldo PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# copy launch info headers
file(COPY ${CMAKE_CURRENT_LIST_DIR}/../../rootsrv/include/LaunchInfo.h  DESTINATION ${CMAKE_CURRENT_LIST_DIR}/include)

# install the library to the sysroot (linker dyldo lmao)
set_target_properties(dyldo PROPERTIES OUTPUT_NAME "ldyldo")

install(TARGETS dyldo RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

add_custom_command(TARGET dyldo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/ldyldo ${SYSROOT_DIR}/boot/sbin/ldyldo)

FILE(GLOB LIBDYLDO_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBDYLDO_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/dyldo)

