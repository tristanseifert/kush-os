# This is an automatically generated file (by idlc). Do not edit.
# Generated from Driverman.idl for interface Driverman at 2021-06-17T13:19:53-0500
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

############################################################
# Structures for message 'StartDevice'
const messageIdStartDevice:UInt64 = 0x6a7cbf9e2efa75f0;

struct StartDeviceRequest {
                            path @0: Text;
}
struct StartDeviceResponse {
                          status @0: Int32;
}

############################################################
# Structures for message 'StopDevice'
const messageIdStopDevice:UInt64 = 0xee8b158787490a80;

struct StopDeviceRequest {
                            path @0: Text;
}
struct StopDeviceResponse {
                          status @0: Int32;
}

