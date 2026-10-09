#include "menu/flick.hpp"
#include "menu/frameworks.hpp"

namespace MCMMemory::Menu
{
     bool Frameworks::RegisterFLICK()
    {
        if (!FUCK::Connect(PRODUCT_NAME)) {
            logger::error("FLICK connection failed; MCM Memory requires FLICK API version {} or newer", FUCK_API_VERSION);
            return false;
        }

        static FLICKTool tool;
        FUCK::RegisterTool(&tool);
        flickVersion = FUCK::GetInterface()->version;
        logger::info("MCM Memory registered with FLICK {}", flickVersion);
        return true;
    }

    void FLICKTool::Draw()
    {
        FUCK::TextUnformatted("MCM Memory is connected to FLICK.");
        FUCK::Spacing();
        FUCK::TextWrapped("Test test test.");
    }
}
