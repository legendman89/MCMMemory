#pragma once

namespace MCMMemory::Menu
{
    class Frameworks
    {
    public:

        static Frameworks* GetSingleton()
        {
            static Frameworks singleton;
            return std::addressof(singleton);
        }

        // Detects available frameworks and registers SKSE Menu Framework at post-load.
        void Register();

        // FLICK connects after game data loads, following its demo's integration flow.
        void RegisterAfterDataLoaded();

        bool HasSKSEMenuFramework() const { return skseVersion > 0.0F; }

        bool HasFLICK() const { return flickVersion > 0; }

    private:

        Frameworks() = default;

        void Detect();

        void RegisterSKSEMenuFramework();

        bool RegisterFLICK();

        float skseVersion{};

        uint32_t flickVersion{};

        bool detected{};
        bool registered{};
        bool flickLoaded{};
    };
}
