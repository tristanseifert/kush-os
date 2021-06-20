# This is an automatically generated file (by idlc). Do not edit.
# Generated from UserClient.idl for interface PciDriverUser at 2021-06-20T13:48:30-0500
@0xfacd9e8b419e773c;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("rpc::_proto::messages");

######################
# Method defenitions #
######################

############################################################
# Structures for message 'GetDeviceAt'
const messageIdGetDeviceAt:UInt64 = 0xd5b64160331233f1;

struct GetDeviceAtRequest {
# Custom serialization type; was 'libpci::BusAddress'
                         address @0: Data;
}
struct GetDeviceAtResponse {
                            path @0: Text;
}

############################################################
# Structures for message 'ReadCfgSpace32'
const messageIdReadCfgSpace32:UInt64 = 0x441bae330756a108;

struct ReadCfgSpace32Request {
# Custom serialization type; was 'libpci::BusAddress'
                         address @0: Data;
                          offset @1: UInt16;
}
struct ReadCfgSpace32Response {
                          result @0: UInt32;
}

############################################################
# Structures for message 'WriteCfgSpace32'
const messageIdWriteCfgSpace32:UInt64 = 0xde92bb2db0b09f5d;

struct WriteCfgSpace32Request {
# Custom serialization type; was 'libpci::BusAddress'
                         address @0: Data;
                          offset @1: UInt16;
                           value @2: UInt32;
}
struct WriteCfgSpace32Response {
}

