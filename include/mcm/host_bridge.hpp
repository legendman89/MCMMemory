#pragma once

#if WITH_BRIDGE
#include "bridge/host_bridge.hpp"
#else
namespace MCMMemory
{
    // A stub if bridge is disabled form compilation. 
    class HostBridge
    {
    public:

        static bool Present() { return GetModuleHandleW(L"MCMBridge.dll") != nullptr; }

        bool Acquire(bool a_restore);

        void Release() {}

        void Cancel() {}

        void Recover() {}

        bool Active() const { return false; }
    };
}
#endif
