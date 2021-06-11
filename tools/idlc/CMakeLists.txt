###############################################################################
# idlc: RPC IDL code generator/compiler
###############################################################################
add_executable(idlc
    src/main.cpp
    src/IDLParser.cpp
    src/InterfaceDescription.cpp
    src/InterfaceDescriptionBuilder.cpp
    src/CodeGenerator.cpp
    src/CodeGenerator+Proto.cpp
    src/CodeGenerator+Server.cpp
    src/CodeGenerator+Client.cpp
    src/CodeGenerator+CppHelpers.cpp
    src/util/MurmurHash2.cpp
)

# libraries
add_subdirectory(lib/PEGTL)
target_link_libraries(idlc PRIVATE taocpp::pegtl)

# install it to the tools bin directory
install(TARGETS idlc RUNTIME DESTINATION ${TOOLS_BIN_DIR})

