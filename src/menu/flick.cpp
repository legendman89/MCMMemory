#include "menu/flick.hpp"
#include "menu/backend.hpp"
#include "menu/activity.hpp"
#include "menu/translate.hpp"
#include "menu/frameworks.hpp"
#include "menu/automation.hpp"
#include "menu/notifications.hpp"

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

        static GUI::FLICK::FLICKTool tool;
        FLK::RegisterTool(&tool);

        static GUI::FLICK::FLICKHUD hud;
        FLK::RegisterWindow(&hud);

        flickVersion = FLK::GetInterface()->version;
        logger::info("MCM Memory registered with FLICK {}", flickVersion);

        return true;
    }
}

namespace MCMMemory::GUI::FLICK
{
    void DrawFLICKHUDMessage(const HUDMessage& a_message, const HUDOptions& a_options, float a_alpha)
    {
        FLK::PushFontScaled(nullptr, static_cast<float>(a_options.fontScale) / 100.0F);

        float textWidth{};
        for (const auto& segment : a_message.segments) {
            textWidth += FLK::CalcTextSize(segment.text.c_str()).x;
        }

        const auto screen = FLK::GetDisplaySize();
        const auto padding = FLK::GetStyleVarVec(ImGuiStyleVar_WindowPadding);
        const float availableWidth = std::max(1.0F, screen.x - padding.x * 2.0F);
        const float fontSize = FLK::GetTextLineHeight() * std::min(1.0F, availableWidth / std::max(1.0F, textWidth));
        FLK::PopFont();
        
        FLK::PushFont(FLK::GetFont(FLK::Font::kRegular), fontSize);
        const float maximumX = std::max(0.0F, screen.x - std::min(textWidth, availableWidth) - padding.x * 2.0F);
        const float maximumY = std::max(0.0F, screen.y - fontSize - padding.y * 2.0F);
        FLK::SetWindowPos(ImVec2{ std::clamp(static_cast<float>(a_options.horizontalOffset), 0.0F, maximumX), std::clamp(static_cast<float>(a_options.verticalOffset), 0.0F, maximumY) });
        FLK::PushStyleColor(ImGuiCol_TextShadow, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });
        for (size_t index = 0; index < a_message.segments.size(); ++index) {
            if (index > 0) {
                FLK::SameLine(0.0F, 0.0F);
            }
            FLK::TextColored(ImVec4{ 1.0F, 1.0F, 1.0F, a_alpha }, "%s", a_message.segments[index].text.c_str());
        }
        FLK::PopStyleColor();
        FLK::PopFont();
    }

    void FLICKTool::Draw()
    {
        FLK::PushStyleColor(ImGuiCol_TextShadow, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });
        FLK::PushStyleColor(ImGuiCol_TextShadowDisabled, ImVec4{ 0.0F, 0.0F, 0.0F, 0.0F });

        GUI::Spacing();

        if (FLK::BeginTabBar("##MCMMemoryTabs")) {

            if (FLK::BeginTabItem(Trans::Tr("Menu.Tab.Profile").c_str())) {

                auto* profileMenu = Menu::ProfileMenu::GetSingleton();
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

                Menu::RenderAutomation();

                GUI::Spacing();
                GUI::Spacing();

                profileMenu->RenderMCMs();
                profileMenu->RenderMCMWindows();
                
                FLK::EndTabItem();
            }

            if (FLK::BeginTabItem(Trans::Tr("Menu.Tab.Activity").c_str())) {
                Menu::ActivityMenu::GetSingleton()->Render();
                FLK::EndTabItem();
            }

            if (FLK::BeginTabItem(Trans::Tr("Menu.Tab.Notifications").c_str())) {
                Menu::RenderNotifications();
                FLK::EndTabItem();
            }

            FLK::EndTabBar();
        }
        
        FLK::PopStyleColor(2);
    }
}
