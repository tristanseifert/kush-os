###############################################################################
# Userspace C library
###############################################################################

list(APPEND c_objs_files
    src/lib_init.c
    # time functions
    src/time/gettimeofday.c
    src/time/time.c
    src/time/strftime.c
    src/time/clock.c
    # string functions
    src/string/memchr.c
    src/string/memclr.c
    src/string/memcmp.c
    src/string/memcpy.c
    src/string/memmove.c
    src/string/memset.c
    # formatting
    src/string/asprintf.c
    src/string/printf.c
    src/string/sscanf.c
    # string manipulation
    src/string/strchr.c
    src/string/strdup.c
    src/string/strlen.c
    src/string/strncat.c
    src/string/strncmp.c
    src/string/strcmp.c
    src/string/strncpy.c
    src/string/strlcat.c
    src/string/strlcpy.c
    src/string/strnlen.c
    src/string/strnstr.c
    src/string/strcoll.c
    src/string/strxfrm.c
    src/string/strcat.c
    src/string/strcpy.c
    # wide strings
    src/string/wcpcpy.c
    src/string/wcpncpy.c
    #    src/string/wcscasecmp.c
    src/string/wcscat.c
    src/string/wcschr.c
    src/string/wcscmp.c
    src/string/wcscpy.c
    src/string/wcslcat.c
    src/string/wcslen.c
    src/string/wcsncat.c
    src/string/wcsncmp.c
    src/string/wcsncpy.c
    src/string/wcsnlen.c
    src/string/wmemset.c
    src/string/wmemmove.c
    src/string/wmemcmp.c
    src/string/wmemcpy.c
    src/string/wmemchr.c
    # string conversions
    src/string/strtoul.c
    src/string/strtoull.c
    src/string/strtol.c
    src/string/strtoll.c
    # syscalls
    src/sys/abort.c
    src/sys/assert.c
    src/sys/atexit.c
    src/sys/exit.c
    # stdlib
    src/stdlib/getopt.c
    src/stdlib/getsubopt.c
    # stdlib math stuff
    src/stdlib/abs.c
    src/stdlib/div.c
    src/stdlib/labs.c
    src/stdlib/ldiv.c
    src/stdlib/llabs.c
    src/stdlib/lldiv.c
    # stdio
    src/stdio/printf.c
    # locale
    src/locale/locale.c
    src/locale/strto.c
    src/locale/isalnum.c
    src/locale/isalpha.c
    src/locale/isascii.c
    src/locale/isblank.c
    src/locale/iscntrl.c
    src/locale/isdigit.c
    src/locale/isgraph.c
    src/locale/islower.c
    src/locale/isprint.c
    src/locale/ispunct.c
    src/locale/isspace.c
    src/locale/isupper.c
    src/locale/isxdigit.c
    src/locale/tolower.c
    src/locale/toupper.c
    src/locale/stdlib.c
    src/locale/string.c
    # wide char support
    src/wchar/conv.c
    src/wchar/strconv.c
    src/wchar/len.c
    src/wchar/wctypes.c
    src/wchar/wctypes_locale.c
    src/wchar/wcscoll.c
    src/wchar/wcsxfrm.c
    # wide string conversions
    src/locale/wcstol.c
    src/locale/wcstoll.c
    src/locale/wcstoul.c
    src/locale/wcstoull.c
    # floating point c onversions
    src/gdtoa/kludge.c
    # memory allocation
    src/mem/malloc.c
    src/mem/fake_sbrk.c
    src/mem/dlmalloc.cpp
    # environment handling
    src/env/getenv.c
    # error handling
    src/error/errno.c
    src/error/strerror.c
    # dynamic linking
    src/dylink/dlopen.c
    src/dylink/dlsym.c
    src/dylink/dlerror.c
    # file IO
    src/file/default_streams.c
    src/file/rpc_file_streams.c
    src/file/fflush.c
    src/file/ferror.c
    src/file/fprintf.c
    src/file/fwrite.c
    src/file/fread.c
    src/file/fseek.c
    src/file/fopen.c
    src/file/fclose.c
    src/file/stat.c
    # POSIX standard stuff
    src/posix/sysconf.c
    # C11 threading implementation
    src/threads/thread_info.c
    src/threads/c11_condvar.c
    src/threads/c11_threads.c
    src/threads/c11_threadlocal.c
    src/threads/c11_mutex.c
    src/threads/sched.c
    # internal data structures
    src/struct/hashmap.c
)

# compile all libc files into an object library (shared for static/dynamic)
add_library(c_objs OBJECT
    ${c_objs_files}
)

target_compile_options(c_objs PRIVATE -flto -fno-exceptions -fno-rtti)
target_compile_definitions(c_objs PRIVATE BUILDING_LIBC=1)

# additional objects for the "full" (aka not the "NO_TLS")  libc
add_library(c_extra_objs OBJECT
    # file IO but with file descriptor numbers
    src/file/fd/map.c
    src/file/fd/read.c
    src/file/fd/write.c
    src/file/fd/close.c
)

target_compile_options(c_extra_objs PRIVATE -flto -fno-exceptions -fno-rtti)
target_compile_definitions(c_extra_objs PRIVATE BUILDING_LIBC=1)

# gdtoa: for floating point conversions
add_library(c_gdtoa OBJECT
    src/gdtoa/dmisc.c
    src/gdtoa/dtoa.c
    src/gdtoa/g__fmt.c
    src/gdtoa/g_ddfmt.c
    src/gdtoa/g_dfmt.c
    src/gdtoa/g_ffmt.c
    src/gdtoa/g_Qfmt.c
    src/gdtoa/g_xfmt.c
    src/gdtoa/g_xLfmt.c
    src/gdtoa/gdtoa.c
    src/gdtoa/gethex.c
    src/gdtoa/gmisc.c
    src/gdtoa/hd_init.c
    src/gdtoa/hexnan.c
    src/gdtoa/misc.c
    src/gdtoa/smisc.c
    src/gdtoa/strtod.c
    src/gdtoa/strtodg.c
    src/gdtoa/strtodI.c
    src/gdtoa/strtof.c
    src/gdtoa/strtoId.c
    src/gdtoa/strtoIdd.c
    src/gdtoa/strtoIf.c
    src/gdtoa/strtoIg.c
    src/gdtoa/strtoIQ.c
    src/gdtoa/strtoIx.c
    src/gdtoa/strtoIxL.c
    src/gdtoa/strtopd.c
    src/gdtoa/strtopdd.c
    src/gdtoa/strtopf.c
    src/gdtoa/strtopQ.c
    src/gdtoa/strtopx.c
    src/gdtoa/strtopxL.c
    src/gdtoa/strtord.c
    src/gdtoa/strtordd.c
    src/gdtoa/strtorf.c
    src/gdtoa/strtorQ.c
    src/gdtoa/strtorx.c
    src/gdtoa/strtorxL.c
    src/gdtoa/sum.c
    src/gdtoa/ulp.c
)
target_compile_options(c_gdtoa PRIVATE -flto)
target_compile_definitions(c_gdtoa PRIVATE BUILDING_LIBC=1)

# copy launch info headers
file(COPY ${CMAKE_CURRENT_LIST_DIR}/../../rootsrv/include/LaunchInfo.h  DESTINATION ${CMAKE_CURRENT_LIST_DIR}/include)

# specify the include directories
target_include_directories(c_objs PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(c_extra_objs PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(c_objs PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(c_extra_objs PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# set up the static and dynamic targets
add_library(c_static STATIC $<TARGET_OBJECTS:c_objs> $<TARGET_OBJECTS:c_extra_objs> $<TARGET_OBJECTS:c_gdtoa>
    src/dylink/dl_iterate_phdr_static.c
    src/threads/tls_static.c
)
target_compile_definitions(c_static PRIVATE BUILDING_LIBC=1)
set_target_properties(c_static PROPERTIES OUTPUT_NAME "c")
target_include_directories(c_static PUBLIC include)
target_link_libraries(c_static PUBLIC system_static)

add_library(c_dynamic SHARED $<TARGET_OBJECTS:c_objs> $<TARGET_OBJECTS:c_extra_objs> $<TARGET_OBJECTS:c_gdtoa>
    src/dylink/dl_iterate_phdr_dynamic.c
    src/threads/tls_dynamic.c
)
target_compile_definitions(c_dynamic PRIVATE BUILDING_LIBC=1)
set_target_properties(c_dynamic PROPERTIES OUTPUT_NAME "c")
target_include_directories(c_dynamic PUBLIC include)
target_link_libraries(c_dynamic PUBLIC system_dynamic rpc_dynamic)
target_compile_options(c_objs PRIVATE -fPIC)

# build the static non-TLS libc (used for dynamic linker)
add_library(c_static_notls STATIC ${c_objs_files} $<TARGET_OBJECTS:c_gdtoa>)

target_include_directories(c_static_notls PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(c_static_notls PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_compile_options(c_static_notls PRIVATE -flto -fno-exceptions -fno-rtti)
target_compile_definitions(c_static_notls PRIVATE BUILDING_LIBC=1 LIBC_NOTLS=1)
target_compile_definitions(c_static_notls PUBLIC LIBC_NOTLS=1)
set_target_properties(c_static_notls PROPERTIES OUTPUT_NAME "c_notls")
target_include_directories(c_static_notls PUBLIC include)
target_link_libraries(c_static_notls PUBLIC system_static)

# build the correct platform-specific code
if(${KERNEL_ARCH} STREQUAL "x86")
    target_compile_definitions(c_objs PRIVATE LIBC_ARCH_X86=1)

    # compile the C library entry point (for static executables)
    add_library(c_crt0T OBJECT
        src/x86/crt0_static.S
        src/x86/crti.c
    )
    target_include_directories(c_crt0T PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

    # compile the C library entry point (for dynamic executables)
    add_library(c_crt0 OBJECT
        src/x86/crt0.S
    )
    target_include_directories(c_crt0 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

    # compile the C library entry point (for shared libraries)
    add_library(c_crt0S OBJECT
        src/x86/crt0_shared.S
    )
    target_include_directories(c_crt0S PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

    # ensure crt0 objects are built and copied to the system library dir (so linker can find them)
    add_dependencies(c_objs c_crt0 c_crt0T c_crt0S)

    add_custom_command(TARGET c_static POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0T.dir/src/x86/crt0_static.S.obj ${SYSROOT_DIR}/lib/crt0T.o
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0T.dir/src/x86/crti.c.obj ${SYSROOT_DIR}/lib/crti.o
    )
    add_custom_command(TARGET c_static POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0.dir/src/x86/crt0.S.obj ${SYSROOT_DIR}/lib/crt0.o
    )
    add_custom_command(TARGET c_static POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0S.dir/src/x86/crt0_shared.S.obj ${SYSROOT_DIR}/lib/crt0S.o
    )

    # export settings to build statically linked executables
    set(C_STATIC_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/src/x86/app_static.ld CACHE FILEPATH "Statically linked C executable linker script")
elseif(${KERNEL_ARCH} STREQUAL "x86_64")
    target_compile_definitions(c_objs PRIVATE LIBC_ARCH_X86_64=1)

    # compile the C library entry point (for static executables)
    add_library(c_crt0T OBJECT
        src/x86_64/crt0_static.S
        src/x86_64/crti.c
    )
    target_include_directories(c_crt0T PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

    # compile the C library entry point (for dynamic executables)
    add_library(c_crt0 OBJECT
        src/x86_64/crt0.S
    )
    target_include_directories(c_crt0 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

    # compile the C library entry point (for shared libraries)
    add_library(c_crt0S OBJECT
        src/x86_64/crt0_shared.S
    )
    target_include_directories(c_crt0S PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

    # ensure crt0 objects are built and copied to the system library dir (so linker can find them)
    add_dependencies(c_objs c_crt0 c_crt0T c_crt0S)

    add_custom_command(TARGET c_static POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0T.dir/src/x86_64/crt0_static.S.obj ${SYSROOT_DIR}/lib/crt0T.o
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0T.dir/src/x86_64/crti.c.obj ${SYSROOT_DIR}/lib/crti.o
    )
    add_custom_command(TARGET c_static POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0.dir/src/x86_64/crt0.S.obj ${SYSROOT_DIR}/lib/crt0.o
    )
    add_custom_command(TARGET c_static POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/c_crt0S.dir/src/x86_64/crt0_shared.S.obj ${SYSROOT_DIR}/lib/crt0S.o
    )

    # export settings to build statically linked executables
    set(C_STATIC_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/src/x86_64/app_static.ld CACHE FILEPATH "Statically linked C executable linker script")

else()
    message(FATAL_ERROR "libc does not know how architecture '${KERNEL_ARCH}'")
endif()

# install library
install(TARGETS c_static LIBRARY)
install(TARGETS c_dynamic LIBRARY)

add_custom_command(TARGET c_dynamic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libc.so ${SYSROOT_DIR}/boot/lib/libc.so)

FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${CMAKE_CURRENT_LIST_DIR}/include/__external_threading DESTINATION ${SYSROOT_DIR}/usr/include)
install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include)
FILE(GLOB LIBC_PUB_SYS_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/*.h")
install(FILES ${LIBC_PUB_SYS_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys)
FILE(GLOB LIBC_LOCALE_SYS_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/locale/*.h")
install(FILES ${LIBC_LOCALE_SYS_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/locale)

