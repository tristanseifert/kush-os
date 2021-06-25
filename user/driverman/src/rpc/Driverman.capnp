# This is an automatically generated file (by idlc). Do not edit.
# Generated from Driverman.idl for interface Driverman at 2021-06-24T17:16:46-0500
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
# Structures for message 'SetDeviceProperty'
const messageIdSetDeviceProperty:UInt64 = 0x4fe09a246da305bc;

struct SetDevicePropertyRequest {
                            path @0: Text;
                             key @1: Text;
                            data @2: Data;
}
struct SetDevicePropertyResponse {
                          status @0: Int32;
}

############################################################
# Structures for message 'GetDeviceProperty'
const messageIdGetDeviceProperty:UInt64 = 0xfaac446645be5520;

struct GetDevicePropertyRequest {
                            path @0: Text;
                             key @1: Text;
}
struct GetDevicePropertyResponse {
                          status @0: Int32;
                            data @1: Data;
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

############################################################
# Structures for message 'Notify'
const messageIdNotify:UInt64 = 0x63ce1044c4349828;

struct NotifyRequest {
                            path @0: Text;
                             key @1: UInt64;
}
struct NotifyResponse {
                          status @0: Int32;
}

