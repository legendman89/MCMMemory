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
        FUCK::PushStyleColor(ImGuiCol_TextShadow, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });
        FUCK::PushStyleColor(ImGuiCol_TextShadowDisabled, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });
        GUI::Spacing();
        auto* profileMenu = ProfileMenu::GetSingleton();
        if (FUCK::BeginTable("##ProfileControls", 3, FUCK::TableFlags::kSizingFixedFit | FUCK::TableFlags::kNoSavedSettings)) {
            FUCK::TableSetupColumn("Profile", FUCK::TableColumnFlags::kWidthFixed);
            FUCK::TableSetupColumn("Spacing", FUCK::TableColumnFlags::kWidthStretch);
            FUCK::TableSetupColumn("Operations", FUCK::TableColumnFlags::kWidthFixed);
            FUCK::TableNextRow();
            FUCK::TableSetColumnIndex(0);
            profileMenu->RenderProfileSelector();
            FUCK::TableSetColumnIndex(2);
            profileMenu->RenderOperationButtons();
            FUCK::EndTable();
        }
        profileMenu->RenderCreateProfileWindow();
        profileMenu->RenderDeleteProfileWindow();
        GUI::Spacing();
        GUI::Spacing();
        RenderAutomation();
        FUCK::PopStyleColor(2);
    }
}
