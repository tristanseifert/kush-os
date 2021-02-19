###############################################################################
# Userspace C library
###############################################################################

add_library(c STATIC
    src/lib_init.c
    # string functions
    src/string/memchr.c
    src/string/memclr.c
    src/string/memcmp.c
    src/string/memcpy.c
    src/string/memmove.c
    src/string/memset.c
    src/string/printf.c
    # string manipulation
    src/string/strchr.c
    src/string/strlen.c
    src/string/strncat.c
    src/string/strncmp.c
    src/string/strcmp.c
    src/string/strncpy.c
    src/string/strnlen.c
    src/string/strnstr.c
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
    src/sys/exit.c
    # stdlib math stuff
    src/stdlib/div.c
    src/stdlib/labs.c
    src/stdlib/ldiv.c
    src/stdlib/llabs.c
    src/stdlib/lldiv.c
    # locale
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
    # stack stuff
    src/stack/stack_guard.c
    # dynamic linking
    src/dylink/dlopen.c
    src/dylink/dlsym.c
    src/dylink/dlerror.c
    src/dylink/dl_iterate_phdr.c
    # file IO
    src/file/default_streams.c
    src/file/fflush.c
    src/file/fprintf.c
)

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
    src/gdtoa/strtodnrp.c
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
target_link_libraries(c PRIVATE c_gdtoa)

# allow the C library to have link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(c PRIVATE "-flto")
endif()

target_compile_options(c PRIVATE -fno-exceptions)

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
    )

    target_compile_definitions(c PRIVATE LIBC_ARCH_X86=1)

    # export settings to build statically linked executables
    set(C_STATIC_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/src/x86/app_static.ld CACHE FILEPATH "Statically linked C executable linker script")
else()
    message(FATAL_ERROR "libc does not know how architecture '${KERNEL_ARCH}'")
endif()

# install the library to the sysroot
install(TARGETS c LIBRARY)

FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include)
FILE(GLOB LIBC_PUB_SYS_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/sys/*.h")
install(FILES ${LIBC_PUB_SYS_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/sys)
