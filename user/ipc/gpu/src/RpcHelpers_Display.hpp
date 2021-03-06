/*
 * This RPC serialization code was autogenerated by idlc (version c8601c6e). DO NOT EDIT!
 * Generated from Display.idl for interface Display at 2021-07-08T01:00:48-0500
 *
 * The structs and methods within are used by the RPC system to serialize and deserialize the
 * arguments and return values on method calls. They work internally in the same way that encoding
 * custom types in RPC messages works.
 *
 * See the full RPC documentation for more details.
 */
#ifndef RPC_HELPERS_GENERATED_10322778102778773913
#define RPC_HELPERS_GENERATED_10322778102778773913

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <span>
#include <string_view>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

#define RPC_USER_TYPES_INCLUDES
#include <DriverSupport/gfx/Types.h>
#undef RPC_USER_TYPES_INCLUDES


namespace rpc {
/**
 * Method to handle a failure to deserialize a field; this will log the failure if built in debug
 * mode.
 */
static inline void HandleDecodeError(const char *typeName, const char *fieldName,
    const uintptr_t offset) {
    fprintf(stderr, "[RPC] Decode error for type %s, field %s at offset $%x\n", typeName, fieldName,
        offset);
}
static inline void HandleDecodeError(const char *typeName, const char *fieldName,
    const uintptr_t offset, const uint32_t blobDataOffset, const uint32_t blobSz) {
    fprintf(stderr, "[RPC] Decode error for type %s, field %s at offset $%x "
        "(blob offset $%x, $%x bytes)\n", typeName, fieldName, offset, blobDataOffset, blobSz);
}

/*
 * Serialization functions for various C++ built in types
 */
inline size_t bytesFor(const std::string &s) {
    return s.length();
}
inline bool serialize(std::span<std::byte> &out, const std::string &str) {
    if(str.empty()) return true;
    else if(out.size() < str.length()) return false;
    memcpy(out.data(), str.c_str(), str.length());
    return true;
}
inline bool deserialize(const std::span<std::byte> &in, std::string &outStr) {
    if(in.empty()) {
        outStr = "";
    } else {
        outStr = std::string(reinterpret_cast<const char *>(in.data()), in.size());
    }
    return true;
}

// XXX: this only works for POD types!
template<typename T>
inline size_t bytesFor(const std::vector<T> &s) {
    return s.size() * sizeof(T);
}
template<typename T>
inline bool serialize(std::span<std::byte> &out, const std::vector<T> &vec) {
    const size_t numBytes = vec.size() * sizeof(T);
    if(out.size() < numBytes) return false;
    memcpy(out.data(), vec.data(), out.size());
    return true;
}
template <typename T>
inline bool deserialize(const std::span<std::byte> &in, std::vector<T> &outVec) {
    const size_t elements = in.size() / sizeof(T);
    outVec.resize(elements);
    memcpy(outVec.data(), in.data(), in.size());
    return true;
}


/*
 * Definitions of serialization structures for messages and message replies. These use the
 * autogenerated stubs to convert to/from the wire format.
 */
namespace internals {
/// Message ids for each of the RPC messages
enum class Type: uint64_t {
                               GetDeviceCapabilities = 0xb3be16a171616697ULL,
                                    SetOutputEnabled = 0xd3ddaaa17cd66af0ULL,
                                       SetOutputMode = 0xf472a05edc874b12ULL,
                                       RegionUpdated = 0xf470173c1b34148aULL,
                                      GetFramebuffer = 0x390defeeb047275dULL,
                                  GetFramebufferInfo = 0xb103a5bbd55c1dc9ULL,
};
/**
 * Request structure for method 'GetDeviceCapabilities'
 */
struct GetDeviceCapabilitiesRequest {

    constexpr static const size_t kElementSizes[0] {
    
    };
    constexpr static const size_t kElementOffsets[0] {
    
    };
    constexpr static const size_t kScalarBytes{0};
    constexpr static const size_t kBlobStartOffset{0};
};
/**
 * Reply structure for method 'GetDeviceCapabilities'
 */
struct GetDeviceCapabilitiesResponse {
    int32_t status;
    uint32_t caps;

    constexpr static const size_t kElementSizes[2] {
     4,  4
    };
    constexpr static const size_t kElementOffsets[2] {
     0,  4
    };
    constexpr static const size_t kScalarBytes{8};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'SetOutputEnabled'
 */
struct SetOutputEnabledRequest {
    bool enabled;

    constexpr static const size_t kElementSizes[1] {
     1
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{1};
    constexpr static const size_t kBlobStartOffset{8};
};
/**
 * Reply structure for method 'SetOutputEnabled'
 */
struct SetOutputEnabledResponse {
    int32_t status;

    constexpr static const size_t kElementSizes[1] {
     4
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{4};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'SetOutputMode'
 */
struct SetOutputModeRequest {
    DriverSupport::gfx::DisplayMode mode;

    constexpr static const size_t kElementSizes[1] {
     8
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{8};
    constexpr static const size_t kBlobStartOffset{8};
};
/**
 * Reply structure for method 'SetOutputMode'
 */
struct SetOutputModeResponse {
    int32_t status;

    constexpr static const size_t kElementSizes[1] {
     4
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{4};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'RegionUpdated'
 */
struct RegionUpdatedRequest {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;

    constexpr static const size_t kElementSizes[4] {
     4,  4,  4,  4
    };
    constexpr static const size_t kElementOffsets[4] {
     0,  4,  8, 12
    };
    constexpr static const size_t kScalarBytes{16};
    constexpr static const size_t kBlobStartOffset{16};
};
/**
 * Reply structure for method 'RegionUpdated'
 */
struct RegionUpdatedResponse {
    int32_t status;

    constexpr static const size_t kElementSizes[1] {
     4
    };
    constexpr static const size_t kElementOffsets[1] {
     0
    };
    constexpr static const size_t kScalarBytes{4};
    constexpr static const size_t kBlobStartOffset{8};
};

/**
 * Request structure for method 'GetFramebuffer'
 */
struct GetFramebufferRequest {

    constexpr static const size_t kElementSizes[0] {
    
    };
    constexpr static const size_t kElementOffsets[0] {
    
    };
    constexpr static const size_t kScalarBytes{0};
    constexpr static const size_t kBlobStartOffset{0};
};
/**
 * Reply structure for method 'GetFramebuffer'
 */
struct GetFramebufferResponse {
    int32_t status;
    uint64_t handle;
    uint64_t size;

    constexpr static const size_t kElementSizes[3] {
     4,  8,  8
    };
    constexpr static const size_t kElementOffsets[3] {
     0,  4, 12
    };
    constexpr static const size_t kScalarBytes{20};
    constexpr static const size_t kBlobStartOffset{24};
};

/**
 * Request structure for method 'GetFramebufferInfo'
 */
struct GetFramebufferInfoRequest {

    constexpr static const size_t kElementSizes[0] {
    
    };
    constexpr static const size_t kElementOffsets[0] {
    
    };
    constexpr static const size_t kScalarBytes{0};
    constexpr static const size_t kBlobStartOffset{0};
};
/**
 * Reply structure for method 'GetFramebufferInfo'
 */
struct GetFramebufferInfoResponse {
    int32_t status;
    uint32_t w;
    uint32_t h;
    uint32_t pitch;

    constexpr static const size_t kElementSizes[4] {
     4,  4,  4,  4
    };
    constexpr static const size_t kElementOffsets[4] {
     0,  4,  8, 12
    };
    constexpr static const size_t kScalarBytes{16};
    constexpr static const size_t kBlobStartOffset{16};
};

} // namespace rpc::internals


inline size_t bytesFor(const internals::GetDeviceCapabilitiesRequest &x) {
    using namespace internals;
    size_t len = GetDeviceCapabilitiesRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetDeviceCapabilitiesRequest &x) {
    using namespace internals;
    uint32_t blobOff = GetDeviceCapabilitiesRequest::kBlobStartOffset;

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetDeviceCapabilitiesRequest &x) {
    using namespace internals;
    if(in.size() < GetDeviceCapabilitiesRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetDeviceCapabilitiesRequest::kBlobStartOffset);

    return true;
}

inline size_t bytesFor(const internals::GetDeviceCapabilitiesResponse &x) {
    using namespace internals;
    size_t len = GetDeviceCapabilitiesResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetDeviceCapabilitiesResponse &x) {
    using namespace internals;
    uint32_t blobOff = GetDeviceCapabilitiesResponse::kBlobStartOffset;
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[0];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[1];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.caps, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetDeviceCapabilitiesResponse &x) {
    using namespace internals;
    if(in.size() < GetDeviceCapabilitiesResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetDeviceCapabilitiesResponse::kBlobStartOffset);
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[0];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }
    {
        const auto off = GetDeviceCapabilitiesResponse::kElementOffsets[1];
        const auto size = GetDeviceCapabilitiesResponse::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.caps, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputEnabledRequest &x) {
    using namespace internals;
    size_t len = SetOutputEnabledRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputEnabledRequest &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputEnabledRequest::kBlobStartOffset;
    {
        const auto off = SetOutputEnabledRequest::kElementOffsets[0];
        const auto size = SetOutputEnabledRequest::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.enabled, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputEnabledRequest &x) {
    using namespace internals;
    if(in.size() < SetOutputEnabledRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputEnabledRequest::kBlobStartOffset);
    {
        const auto off = SetOutputEnabledRequest::kElementOffsets[0];
        const auto size = SetOutputEnabledRequest::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.enabled, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputEnabledResponse &x) {
    using namespace internals;
    size_t len = SetOutputEnabledResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputEnabledResponse &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputEnabledResponse::kBlobStartOffset;
    {
        const auto off = SetOutputEnabledResponse::kElementOffsets[0];
        const auto size = SetOutputEnabledResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputEnabledResponse &x) {
    using namespace internals;
    if(in.size() < SetOutputEnabledResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputEnabledResponse::kBlobStartOffset);
    {
        const auto off = SetOutputEnabledResponse::kElementOffsets[0];
        const auto size = SetOutputEnabledResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputModeRequest &x) {
    using namespace internals;
    size_t len = SetOutputModeRequest::kBlobStartOffset;
    len += bytesFor(x.mode);

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputModeRequest &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputModeRequest::kBlobStartOffset;
    {
        const auto off = SetOutputModeRequest::kElementOffsets[0];
        const auto size = SetOutputModeRequest::kElementSizes[0];
        auto range = out.subspan(off, size);
        const uint32_t blobSz = bytesFor(x.mode);
        const uint32_t blobDataOffset = blobOff;
        auto blobRange = out.subspan(blobDataOffset, blobSz);
        if(!serialize(blobRange, x.mode)) return false;
        blobOff += blobSz;
        memcpy(range.data(), &blobDataOffset, sizeof(blobDataOffset));
        memcpy(range.data()+sizeof(blobDataOffset), &blobSz, sizeof(blobSz));
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputModeRequest &x) {
    using namespace internals;
    if(in.size() < SetOutputModeRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputModeRequest::kBlobStartOffset);
    {
        const auto off = SetOutputModeRequest::kElementOffsets[0];
        const auto size = SetOutputModeRequest::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        uint32_t blobSz{0}, blobDataOffset{0};
        memcpy(&blobDataOffset, range.data(), sizeof(blobDataOffset));
        memcpy(&blobSz, range.data()+sizeof(blobDataOffset), sizeof(blobSz));
        auto blobRange = in.subspan(blobDataOffset, blobSz);
       if(!deserialize(blobRange, x.mode)) {
            HandleDecodeError("SetOutputModeRequest", "mode", off, blobDataOffset, blobSz);
            return false;
        }
    }

    return true;
}

inline size_t bytesFor(const internals::SetOutputModeResponse &x) {
    using namespace internals;
    size_t len = SetOutputModeResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::SetOutputModeResponse &x) {
    using namespace internals;
    uint32_t blobOff = SetOutputModeResponse::kBlobStartOffset;
    {
        const auto off = SetOutputModeResponse::kElementOffsets[0];
        const auto size = SetOutputModeResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::SetOutputModeResponse &x) {
    using namespace internals;
    if(in.size() < SetOutputModeResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(SetOutputModeResponse::kBlobStartOffset);
    {
        const auto off = SetOutputModeResponse::kElementOffsets[0];
        const auto size = SetOutputModeResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::RegionUpdatedRequest &x) {
    using namespace internals;
    size_t len = RegionUpdatedRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::RegionUpdatedRequest &x) {
    using namespace internals;
    uint32_t blobOff = RegionUpdatedRequest::kBlobStartOffset;
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[0];
        const auto size = RegionUpdatedRequest::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.x, range.size());
    }
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[1];
        const auto size = RegionUpdatedRequest::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.y, range.size());
    }
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[2];
        const auto size = RegionUpdatedRequest::kElementSizes[2];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.w, range.size());
    }
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[3];
        const auto size = RegionUpdatedRequest::kElementSizes[3];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.h, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::RegionUpdatedRequest &x) {
    using namespace internals;
    if(in.size() < RegionUpdatedRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(RegionUpdatedRequest::kBlobStartOffset);
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[0];
        const auto size = RegionUpdatedRequest::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.x, range.data(), range.size());
    }
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[1];
        const auto size = RegionUpdatedRequest::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.y, range.data(), range.size());
    }
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[2];
        const auto size = RegionUpdatedRequest::kElementSizes[2];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.w, range.data(), range.size());
    }
    {
        const auto off = RegionUpdatedRequest::kElementOffsets[3];
        const auto size = RegionUpdatedRequest::kElementSizes[3];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.h, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::RegionUpdatedResponse &x) {
    using namespace internals;
    size_t len = RegionUpdatedResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::RegionUpdatedResponse &x) {
    using namespace internals;
    uint32_t blobOff = RegionUpdatedResponse::kBlobStartOffset;
    {
        const auto off = RegionUpdatedResponse::kElementOffsets[0];
        const auto size = RegionUpdatedResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::RegionUpdatedResponse &x) {
    using namespace internals;
    if(in.size() < RegionUpdatedResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(RegionUpdatedResponse::kBlobStartOffset);
    {
        const auto off = RegionUpdatedResponse::kElementOffsets[0];
        const auto size = RegionUpdatedResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::GetFramebufferRequest &x) {
    using namespace internals;
    size_t len = GetFramebufferRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetFramebufferRequest &x) {
    using namespace internals;
    uint32_t blobOff = GetFramebufferRequest::kBlobStartOffset;

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetFramebufferRequest &x) {
    using namespace internals;
    if(in.size() < GetFramebufferRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetFramebufferRequest::kBlobStartOffset);

    return true;
}

inline size_t bytesFor(const internals::GetFramebufferResponse &x) {
    using namespace internals;
    size_t len = GetFramebufferResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetFramebufferResponse &x) {
    using namespace internals;
    uint32_t blobOff = GetFramebufferResponse::kBlobStartOffset;
    {
        const auto off = GetFramebufferResponse::kElementOffsets[0];
        const auto size = GetFramebufferResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }
    {
        const auto off = GetFramebufferResponse::kElementOffsets[1];
        const auto size = GetFramebufferResponse::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.handle, range.size());
    }
    {
        const auto off = GetFramebufferResponse::kElementOffsets[2];
        const auto size = GetFramebufferResponse::kElementSizes[2];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.size, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetFramebufferResponse &x) {
    using namespace internals;
    if(in.size() < GetFramebufferResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetFramebufferResponse::kBlobStartOffset);
    {
        const auto off = GetFramebufferResponse::kElementOffsets[0];
        const auto size = GetFramebufferResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }
    {
        const auto off = GetFramebufferResponse::kElementOffsets[1];
        const auto size = GetFramebufferResponse::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.handle, range.data(), range.size());
    }
    {
        const auto off = GetFramebufferResponse::kElementOffsets[2];
        const auto size = GetFramebufferResponse::kElementSizes[2];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.size, range.data(), range.size());
    }

    return true;
}

inline size_t bytesFor(const internals::GetFramebufferInfoRequest &x) {
    using namespace internals;
    size_t len = GetFramebufferInfoRequest::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetFramebufferInfoRequest &x) {
    using namespace internals;
    uint32_t blobOff = GetFramebufferInfoRequest::kBlobStartOffset;

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetFramebufferInfoRequest &x) {
    using namespace internals;
    if(in.size() < GetFramebufferInfoRequest::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetFramebufferInfoRequest::kBlobStartOffset);

    return true;
}

inline size_t bytesFor(const internals::GetFramebufferInfoResponse &x) {
    using namespace internals;
    size_t len = GetFramebufferInfoResponse::kBlobStartOffset;

    return len;
}
inline bool serialize(std::span<std::byte> &out, const internals::GetFramebufferInfoResponse &x) {
    using namespace internals;
    uint32_t blobOff = GetFramebufferInfoResponse::kBlobStartOffset;
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[0];
        const auto size = GetFramebufferInfoResponse::kElementSizes[0];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.status, range.size());
    }
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[1];
        const auto size = GetFramebufferInfoResponse::kElementSizes[1];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.w, range.size());
    }
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[2];
        const auto size = GetFramebufferInfoResponse::kElementSizes[2];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.h, range.size());
    }
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[3];
        const auto size = GetFramebufferInfoResponse::kElementSizes[3];
        auto range = out.subspan(off, size);
        memcpy(range.data(), &x.pitch, range.size());
    }

    return true;
}
inline bool deserialize(const std::span<std::byte> &in, internals::GetFramebufferInfoResponse &x) {
    using namespace internals;
    if(in.size() < GetFramebufferInfoResponse::kScalarBytes) return false;
    const auto blobRegion = in.subspan(GetFramebufferInfoResponse::kBlobStartOffset);
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[0];
        const auto size = GetFramebufferInfoResponse::kElementSizes[0];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.status, range.data(), range.size());
    }
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[1];
        const auto size = GetFramebufferInfoResponse::kElementSizes[1];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.w, range.data(), range.size());
    }
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[2];
        const auto size = GetFramebufferInfoResponse::kElementSizes[2];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.h, range.data(), range.size());
    }
    {
        const auto off = GetFramebufferInfoResponse::kElementOffsets[3];
        const auto size = GetFramebufferInfoResponse::kElementSizes[3];
        auto range = in.subspan(off, size);
        if(range.empty() || range.size() != size) return false;
        memcpy(&x.pitch, range.data(), range.size());
    }

    return true;
}

}; // namespace rpc

#pragma clang diagnostic push

#endif // defined(RPC_HELPERS_GENERATED_10322778102778773913)
