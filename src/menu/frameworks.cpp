#include "menu/frameworks.hpp"
#include "menu/API/SKSEMenuFramework.h"

#include <cstring>

namespace MCMMemory::Menu
{
    void Frameworks::Detect()
    {
        skseVersion = SKSEMenuFramework::GetMenuFrameworkVersion();

        flickVersion = 0;

        if (HasSKSEMenuFramework()) {
            logger::info("Found SKSE Menu Framework version {}", skseVersion);
        }

        const auto module = GetModuleHandleW(L"FUCK.dll");
        if (!module) {
            logger::info("FLICK is not loaded");
            return;
        }

        using RequestInterface = void* (*)();
        const auto request = reinterpret_cast<RequestInterface>(GetProcAddress(module, "RequestFUCK"));
        if (!request) {
            logger::warn("FLICK is loaded but RequestFUCK is unavailable");
            return;
        }

        const auto* api = request();
        if (!api) {
            logger::warn("FLICK returned no API interface");
            return;
        }

        // The FLICK API version is a 32-bit unsigned at the start of the interface.
        // If that member moved elsewhere we are FUCKed.
        std::memcpy(&flickVersion, api, sizeof(flickVersion));
        if (HasFLICK()) {
            logger::info("Found FLICK version {}", flickVersion);
        }
        else {
            logger::warn("FLICK returned an invalid API version 0");
        }
    }
}
