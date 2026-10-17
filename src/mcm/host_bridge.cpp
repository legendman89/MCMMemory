#include "mcm/host_bridge.hpp"

#if !WITH_BRIDGE
namespace MCMMemory
{
    bool HostBridge::Acquire(bool)
    {
        if (Present()) {
            logger::error("MCMBridge is loaded, but this build has no Bridge support; backup and restore are disabled");
            return false;
        }
        return true;
    }
}
#endif
