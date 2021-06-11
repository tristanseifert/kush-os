# This is an automatically generated file (by idlc). Do not edit.
# Generated from Driverman.idl for interface Driverman at 2021-06-11T01:44:27-0500
@0x9c46a74e95804d52;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("rpc::_proto::messages");

######################
# Method defenitions #
######################

############################################################
# Structures for message 'addDevice'
const messageIdAddDevice:UInt64 = 0xe2cd5678129683fe;

struct AddDeviceRequest {
                          parent @0: Text;
                        driverId @1: Text;
                         auxData @2: Data;
}
struct AddDeviceResponse {
                            path @0: Text;
}

############################################################
# Structures for message 'setDeviceProperty'
const messageIdSetDeviceProperty:UInt64 = 0x9bd9cc73d64e045b;

struct SetDevicePropertyRequest {
                            path @0: Text;
                             key @1: Text;
                            data @2: Data;
}
struct SetDevicePropertyResponse {
}

############################################################
# Structures for message 'getDeviceProperty'
const messageIdGetDeviceProperty:UInt64 = 0x633375a0eade91a1;

struct GetDevicePropertyRequest {
                            path @0: Text;
                             key @1: Text;
}
struct GetDevicePropertyResponse {
                            data @0: Data;
}

