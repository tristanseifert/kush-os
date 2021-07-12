###################################################################################################
# Window Server
#
# Responsible for compositing the desktop together from various different applications' windows, as
# well as distributing events to them.
###################################################################################################
add_executable(windowserver
    src/main.cpp
    src/Log.cpp
    # rpc interface
    src/rpc/RpcServer.cpp
    src/rpc/Server_WindowServer.cpp
    # compositor
    src/compositor/Compositor.cpp
)

target_compile_options(windowserver PRIVATE -flto -fno-rtti -fno-exceptions)
#target_link_options(windowserver PRIVATE -s)

target_include_directories(windowserver PRIVATE include)
target_include_directories(windowserver PRIVATE src)

target_link_libraries(windowserver PRIVATE DriverSupport::Graphics rpc)

install(TARGETS windowserver RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

