###############################################################################
# mkinit: Builds an initialization bundle
###############################################################################
add_executable(mkinit
    src/main.cpp
    src/InitBundle.cpp
)

# we require the lzfse library: on macOS this is available from brew
target_link_libraries(mkinit PRIVATE lzfse)

# install it to the tools bin directory
install(TARGETS mkinit RUNTIME DESTINATION ${TOOLS_BIN_DIR})

