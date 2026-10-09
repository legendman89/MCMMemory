#pragma once

namespace MCMMemory::Menu
{
    struct Frameworks
    {
        void Detect();

        bool HasSKSEMenuFramework() const { return skseVersion > 0.0F; }

        bool HasFLICK() const { return flickVersion > 0; }

        float skseVersion{};

        uint32_t flickVersion{};
    };
}
