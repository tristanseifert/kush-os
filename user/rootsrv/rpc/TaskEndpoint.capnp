@0x968e91fff332f596;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("rpc::TaskEndpoint");

struct CreateRequest {
    path        @0 :Text;
    args        @1 :List(Text);
}

struct CreateReply {
    status      @0: Int32;
    handle      @1: UInt64;
}

# message type constants
const kTypeCreateRequest : UInt32 = 0x54534B43;
const kTypeCreateReply : UInt32 = 0xD4534B43;

