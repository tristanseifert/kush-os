# This is an automatically generated file (by idlc). Do not edit.
# Generated from Filesystem.idl for interface Filesystem at 2021-06-24T01:30:24-0500
@0xff41ee7b60f358db;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("rpc::_proto::messages");

######################
# Method defenitions #
######################

############################################################
# Structures for message 'OpenFile'
const messageIdOpenFile:UInt64 = 0xdccae6ca6448b367;

struct OpenFileRequest {
                            path @0: Text;
                            mode @1: UInt32;
}
struct OpenFileResponse {
                          status @0: Int32;
                          handle @1: UInt64;
                        fileSize @2: UInt64;
}

############################################################
# Structures for message 'SlowRead'
const messageIdSlowRead:UInt64 = 0xfb13530a05a3aaf2;

struct SlowReadRequest {
                          handle @0: UInt64;
                          offset @1: UInt64;
                        numBytes @2: UInt16;
}
struct SlowReadResponse {
                          status @0: Int32;
                            data @1: Data;
}

############################################################
# Structures for message 'CloseFile'
const messageIdCloseFile:UInt64 = 0xbe7b08fc61ccb369;

struct CloseFileRequest {
                          handle @0: UInt64;
}
struct CloseFileResponse {
                          status @0: Int32;
}

