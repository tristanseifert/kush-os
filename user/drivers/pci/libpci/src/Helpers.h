#ifndef LIBDRIVER_CLIENTS_PCI_HELPERS_H
#define LIBDRIVER_CLIENTS_PCI_HELPERS_H

#include <cstddef>
#include <span>

#include "userclient/UserClientTypes.h"

namespace libpci::internal {
/// Decodes the aux data blob into a bus address.
bool DecodeAddressInfo(const std::span<std::byte> &data, BusAddress &outAddr);
}

#endif
