#ifndef INIT_INIT_H
#define INIT_INIT_H

#include <memory>

namespace init {
class Bundle;

/// Sets up servers
void SetupServers(std::shared_ptr<Bundle> &bundle);

}

#endif
