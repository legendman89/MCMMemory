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
        if (!FLK::Connect(PRODUCT_NAME)) {
            logger::error("FLICK connection failed; MCM Memory requires FLICK API version {} or newer", FUCK_API_VERSION);
            return false;
        }

#define GUI_REGISTER_FLICK(name, result, args) GUI::RegisterFunction<GUI::Function::name>(GUI::FLICK::name);
        FOREACH_GUI_FUNCTIONS(GUI_REGISTER_FLICK)
#undef GUI_REGISTER_FLICK

        Trans::GetTranslator().Load();
        static FLICKTool tool;
        FLK::RegisterTool(&tool);
        flickVersion = FLK::GetInterface()->version;
        logger::info("MCM Memory registered with FLICK {}", flickVersion);
        return true;
    }

    void FLICKTool::OnOpen()
    {
        ProfileMenu::GetSingleton()->RefreshProfileNames();
    }

    void FLICKTool::Draw()
    {
        FLK::PushStyleColor(ImGuiCol_TextShadow, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });
        FLK::PushStyleColor(ImGuiCol_TextShadowDisabled, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });

        GUI::Spacing();

        auto* profileMenu = ProfileMenu::GetSingleton();
        if (FLK::BeginTable("##ProfileControls", 3, FLK::TableFlags::kSizingFixedFit | FLK::TableFlags::kNoSavedSettings)) {
            FLK::TableSetupColumn("Profile", FLK::TableColumnFlags::kWidthFixed);
            FLK::TableSetupColumn("Spacing", FLK::TableColumnFlags::kWidthStretch);
            FLK::TableSetupColumn("Operations", FLK::TableColumnFlags::kWidthFixed);
            FLK::TableNextRow();
            FLK::TableSetColumnIndex(0);
            profileMenu->RenderProfileSelector();
            FLK::TableSetColumnIndex(2);
            profileMenu->RenderOperationButtons();
            FLK::EndTable();
        }

        profileMenu->RenderCreateProfileWindow();
        profileMenu->RenderDeleteProfileWindow();

        GUI::Spacing();
        GUI::Spacing();

        RenderAutomation();

        GUI::Spacing();
        GUI::Spacing();

        profileMenu->RenderMCMs();
        profileMenu->RenderMCMWindows();
        
        FLK::PopStyleColor(2);
    }
}
