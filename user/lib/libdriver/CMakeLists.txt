###############################################################################
# Driver library
#
# Helpers for writing driver servers.
###############################################################################
add_library(driver SHARED
    # serialization
    src/serialize/base85.c
)

# allow the library to have link time optimization
target_compile_options(driver PRIVATE -flto)
target_compile_options(driver PRIVATE -fno-exceptions -fno-rtti)
target_link_options(driver PUBLIC -fno-exceptions -fno-rtti)

# specify the include directories
target_include_directories(driver PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(driver PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# for rpc stuff we need Cista
target_link_libraries(driver PRIVATE cista)

# install the library to the sysroot
set_target_properties(driver PROPERTIES OUTPUT_NAME "driver")
install(TARGETS driver LIBRARY)

# copy the driver headers
FILE(GLOB LIBDRIVER_PUB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/driver/*.h*")
install(FILES ${LIBDRIVER_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/driver)

