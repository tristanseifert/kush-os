###############################################################################
# RPC library
#
# Provides interfaces to some RPC services.
###############################################################################

add_library(rpc_objs OBJECT
    src/helpers/Send.cpp
    src/rpc/Dispensary.cpp
    # file io
    src/file/Connection.cpp
    src/file/Open.cpp
    src/file/Read.cpp
)
target_compile_options(rpc_objs PRIVATE -fPIC)

# allow the library to have link time optimization
target_compile_options(rpc_objs PRIVATE -flto)
target_compile_options(rpc_objs PRIVATE -fno-exceptions -fno-rtti)
target_link_options(rpc_objs PUBLIC -fno-exceptions -fno-rtti -nostartfiles)
# specify the include directories
target_include_directories(rpc_objs PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(rpc_objs PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# for RPC stuff we need Cista
target_link_libraries(rpc_objs PRIVATE cista)

# set up the static and dynamic targets
add_library(rpc_static STATIC $<TARGET_OBJECTS:rpc_objs>)
set_target_properties(rpc_static PROPERTIES OUTPUT_NAME "rpc")
target_include_directories(rpc_static PUBLIC include)
target_link_libraries(rpc_static PUBLIC system_static)

add_library(rpc_dynamic SHARED $<TARGET_OBJECTS:rpc_objs>)
set_target_properties(rpc_dynamic PROPERTIES OUTPUT_NAME "rpc")
target_include_directories(rpc_dynamic PUBLIC include)
target_link_options(rpc_dynamic PRIVATE -nostartfiles)
#target_link_libraries(rpc_dynamic PUBLIC system_dynamic)

# install the library to the sysroot
install(TARGETS rpc_static LIBRARY)
install(TARGETS rpc_dynamic LIBRARY)

# copy the rpc headers
FILE(GLOB LIBRPC_RPC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rpc/*.h*")
install(FILES ${LIBRPC_RPC_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/rpc)

