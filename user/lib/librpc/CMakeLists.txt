###############################################################################
# RPC library
#
# Provides interfaces to some RPC services.
###############################################################################
find_package(CapnProto)

# compile all Cap'n Proto schemas
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS
    # task endpoint
    rpc/rootsrv/TaskEndpoint.capnp
)

add_library(rpc_objs_small OBJECT
    src/helpers/Send.cpp
    src/rpc/Dispensary.cpp
    # file io
    src/file/Connection.cpp
    src/file/Open.cpp
    src/file/Read.cpp
)

add_library(rpc_objs OBJECT
    # task creation
    src/task/Connection.cpp
    src/task/Create.cpp
    # generated Cap'n Proto files
    ${CAPNP_SRCS}
)
target_include_directories(rpc_objs PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# allow the library to have link time optimization
target_compile_options(rpc_objs PRIVATE -fPIC -flto -fno-exceptions -fno-rtti)
target_compile_options(rpc_objs_small PRIVATE -fPIC -flto -fno-exceptions -fno-rtti)
# specify the include directories
target_include_directories(rpc_objs PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(rpc_objs_small PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(rpc_objs PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(rpc_objs_small PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# set up the static and dynamic targets
add_library(rpc_static STATIC $<TARGET_OBJECTS:rpc_objs_small> $<TARGET_OBJECTS:rpc_objs>)
set_target_properties(rpc_static PROPERTIES OUTPUT_NAME "rpc")
target_include_directories(rpc_static PUBLIC include)
target_link_libraries(rpc_static PUBLIC system_static)

add_library(rpc_dynamic SHARED $<TARGET_OBJECTS:rpc_objs_small> $<TARGET_OBJECTS:rpc_objs>)
set_target_properties(rpc_dynamic PROPERTIES OUTPUT_NAME "rpc")
target_include_directories(rpc_dynamic PUBLIC include)

# a smaller static library
add_library(rpc_static_small STATIC $<TARGET_OBJECTS:rpc_objs_small> $<TARGET_OBJECTS:rpc_objs>)
target_include_directories(rpc_static_small PUBLIC include)
target_link_libraries(rpc_static_small PUBLIC system_static)

# Cap'n Proto provides our RPC mechanism
target_link_libraries(rpc_static PRIVATE capnp)
target_link_libraries(rpc_dynamic PRIVATE capnp)

# install the library to the sysroot
install(TARGETS rpc_static LIBRARY)
install(TARGETS rpc_dynamic LIBRARY)

add_custom_command(TARGET rpc_dynamic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/librpc.so ${SYSROOT_DIR}/boot/lib/librpc.so)

# copy the rpc headers
FILE(GLOB LIBRPC_RPC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rpc/*.h*")
install(FILES ${LIBRPC_RPC_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/rpc)

