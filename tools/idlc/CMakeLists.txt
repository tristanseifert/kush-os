###############################################################################
# idlc: RPC IDL code generator/compiler
###############################################################################
add_executable(idlc
    src/main.cpp
    src/IDLParser.cpp
    src/InterfaceDescription.cpp
)

# libraries
add_subdirectory(lib/PEGTL)
target_link_libraries(idlc PRIVATE taocpp::pegtl)

# install it to the tools bin directory
install(TARGETS idlc RUNTIME DESTINATION ${TOOLS_BIN_DIR})

