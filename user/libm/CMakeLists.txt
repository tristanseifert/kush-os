###############################################################################
# Userspace C maths library
###############################################################################

add_library(m STATIC
    src/main.c
)

# allow the library to have link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(m PRIVATE "-flto")
endif()

# set required compiler flags
target_compile_definitions(m PRIVATE BUILDING_LIBM=1)

# specify the include directories
target_include_directories(m PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(m PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# install the library to the sysroot
install(TARGETS m ARCHIVE DESTINATION ${SYSROOT_DIR}/lib)
install(TARGETS m LIBRARY DESTINATION ${SYSROOT_DIR}/lib)

FILE(GLOB LIBC_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBC_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include)

