#include "menu/flick.hpp"
#include "menu/backend.hpp"
#include "menu/frameworks.hpp"
#include "menu/automation.hpp"
#include "menu/profile.hpp"
#include "menu/translate.hpp"

namespace MCMMemory::Menu
{
     bool Frameworks::RegisterFLICK()
    {
        if (!FUCK::Connect(PRODUCT_NAME)) {
            logger::error("FLICK connection failed; MCM Memory requires FLICK API version {} or newer", FUCK_API_VERSION);
            return false;
        }

#define GUI_REGISTER_FLICK(name, result, args) GUI::RegisterFunction<GUI::Function::name>(GUI::FLICK::name);
        FOREACH_GUI_FUNCTIONS(GUI_REGISTER_FLICK)
#undef GUI_REGISTER_FLICK

        Trans::GetTranslator().Load();
        static FLICKTool tool;
        FUCK::RegisterTool(&tool);
        flickVersion = FUCK::GetInterface()->version;
        logger::info("MCM Memory registered with FLICK {}", flickVersion);
        return true;
    }

    void FLICKTool::OnOpen()
    {
        ProfileMenu::GetSingleton()->RefreshProfileNames();
    }

    void FLICKTool::Draw()
    {
        GUI::Spacing();
        auto* profileMenu = ProfileMenu::GetSingleton();
        profileMenu->RenderProfileSelector();
        profileMenu->RenderCreateProfileWindow();
        profileMenu->RenderDeleteProfileWindow();
        GUI::Spacing();
        GUI::Spacing();
        RenderAutomation();
    }
}
