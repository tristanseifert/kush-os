# This is an automatically generated file (by idlc). Do not edit.
# Generated from DiskDriver.idl for interface DiskDriver at 2021-06-21T02:00:07-0500
@0xecd58c2475820a5b;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("rpc::_proto::messages");

######################
# Method defenitions #
######################

############################################################
# Structures for message 'GetCapacity'
const messageIdGetCapacity:UInt64 = 0x91df49e5f38b0cb5;

struct GetCapacityRequest {
                          diskId @0: UInt64;
}
struct GetCapacityResponse {
                          status @0: Int32;
                      sectorSize @1: UInt32;
                      numSectors @2: UInt64;
}

############################################################
# Structures for message 'OpenSession'
const messageIdOpenSession:UInt64 = 0xf4e1aa89aee2c5f;

struct OpenSessionRequest {
}
struct OpenSessionResponse {
                          status @0: Int32;
                    sessionToken @1: UInt64;
                    regionHandle @2: UInt64;
                      regionSize @3: UInt64;
                     numCommands @4: UInt64;
}

############################################################
# Structures for message 'CloseSession'
const messageIdCloseSession:UInt64 = 0xbdac3777974760fb;

struct CloseSessionRequest {
                         session @0: UInt64;
}
struct CloseSessionResponse {
                          status @0: Int32;
}

############################################################
# Structures for message 'CreateReadBuffer'
const messageIdCreateReadBuffer:UInt64 = 0x5c63169ecba56263;

struct CreateReadBufferRequest {
                         session @0: UInt64;
                   requestedSize @1: UInt64;
}
struct CreateReadBufferResponse {
                          status @0: Int32;
                   readBufHandle @1: UInt64;
                  readBufMaxSize @2: UInt64;
}

############################################################
# Structures for message 'CreateWriteBuffer'
const messageIdCreateWriteBuffer:UInt64 = 0x2647106809f005fc;

struct CreateWriteBufferRequest {
                         session @0: UInt64;
                   requestedSize @1: UInt64;
}
struct CreateWriteBufferResponse {
                          status @0: Int32;
                  writeBufHandle @1: UInt64;
                 writeBufMaxSize @2: UInt64;
}

############################################################
# Structures for message 'ExecuteCommand'
const messageIdExecuteCommand:UInt64 = 0xaae5bbef0049e019;

struct ExecuteCommandRequest {
                         session @0: UInt64;
                            slot @1: UInt32;
}
# Message is async, no response struct needed

############################################################
# Structures for message 'ReleaseReadCommand'
const messageIdReleaseReadCommand:UInt64 = 0xdcc360757768916a;

struct ReleaseReadCommandRequest {
                         session @0: UInt64;
                            slot @1: UInt32;
}
# Message is async, no response struct needed

############################################################
# Structures for message 'AllocWriteMemory'
const messageIdAllocWriteMemory:UInt64 = 0x3dc1fae0d30f6af6;

struct AllocWriteMemoryRequest {
                         session @0: UInt64;
                  bytesRequested @1: UInt64;
}
struct AllocWriteMemoryResponse {
                          status @0: Int32;
                          offset @1: UInt64;
                  bytesAllocated @2: UInt64;
}

