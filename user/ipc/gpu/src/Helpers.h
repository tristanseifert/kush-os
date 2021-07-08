/**
 * Various helper routines for working with graphics drivers
 */
#ifndef DRIVERSUPPORT_GFX_HELPERS_H
#define DRIVERSUPPORT_GFX_HELPERS_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <vector>

namespace DriverSupport::gfx {
/// Decodes a connection info property value into an RPC port and device id.
std::tuple<uintptr_t, uint32_t> DecodeConnectionInfo(const std::span<std::byte> &in);
/// Encodes an RPC port handle and device id into a connection info blob.
bool EncodeConnectionInfo(const uintptr_t port, const uint32_t displayId,
        std::vector<std::byte> &out);
}

#endif
