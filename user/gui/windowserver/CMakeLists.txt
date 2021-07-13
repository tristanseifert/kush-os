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
    src/compositor/CursorHandler.cpp
)

target_compile_options(windowserver PRIVATE -flto -fno-rtti -fno-exceptions)

target_include_directories(windowserver PRIVATE include)
target_include_directories(windowserver PRIVATE src)

target_link_libraries(windowserver PRIVATE tomlplusplus::tomlplusplus Gui::Graphics
    DriverSupport::Graphics rpc)

install(TARGETS windowserver RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

# install cursor files
install(FILES assets/cursors.toml DESTINATION ${SYSROOT_DIR}/System/Data/windowserver)

FILE(GLOB WINDOWSERVER_CURSOR_FILES "${CMAKE_CURRENT_LIST_DIR}/assets/cursors/*.png")
install(FILES ${WINDOWSERVER_CURSOR_FILES} DESTINATION ${SYSROOT_DIR}/System/Data/windowserver/cursors)
