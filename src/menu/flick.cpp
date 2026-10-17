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

        static GUI::FLICK::FLICKHUD warning(true);
        FLK::RegisterWindow(&warning);

        flickVersion = FLK::GetInterface()->version;
        logger::info("MCM Memory registered with FLICK {}", flickVersion);

        return true;
    }
}

namespace MCMMemory::GUI::FLICK
{
    using ::ImVec2;
    using ::ImVec4;

    void FLICKHUD::Draw()
    {
        auto* ui = RE::UI::GetSingleton();
        const bool blocked = !FLK::IsMenuOpen() && ui && (ui->GameIsPaused() || ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) || ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME));
        if (warningWindow) {
            HUD::GetSingleton()->RenderWarning(blocked, DrawFLICKHUDWarning);
        }
        else {
            HUD::GetSingleton()->RenderNotifications(blocked, DrawFLICKHUDMessage);
        }
    }

    void DrawFLICKHUDMessage(const HUDMessage& a_message, const HUDOptions& a_options, float a_alpha)
    {
        if (a_message.segments.empty() || a_alpha <= 0.0F) {
            return;
        }

        FLK::PushFontScaled(nullptr, static_cast<float>(a_options.fontScale) / 100.0F);

        float textWidth{};
        for (const auto& segment : a_message.segments) {
            textWidth += FLK::CalcTextSize(segment.text.c_str()).x;
        }

        const auto screen = FLK::GetDisplaySize();
        const ImVec2 padding{ HUDHorizontalPadding, HUDVerticalPadding };
        const float availableWidth = std::max(1.0F, screen.x - padding.x * 2.0F);
        const float fontSize = FLK::GetTextLineHeight() * std::min(1.0F, availableWidth / std::max(1.0F, textWidth));
        FLK::PopFont();
        
        FLK::PushFont(FLK::GetFont(FLK::Font::kRegular), fontSize);
        const ImVec2 backgroundSize{ std::min(textWidth, availableWidth) + padding.x * 2.0F, fontSize + padding.y * 2.0F };
        const float maximumX = std::max(0.0F, screen.x - backgroundSize.x);
        const float maximumY = std::max(0.0F, screen.y - backgroundSize.y);
        FLK::SetWindowPos(ImVec2{ std::clamp(static_cast<float>(a_options.horizontalOffset), 0.0F, maximumX), std::clamp(static_cast<float>(a_options.verticalOffset), 0.0F, maximumY) });
        FLK::SetCursorPos(ImVec2{ 0.0F, 0.0F });
        const auto backgroundMin = FLK::GetCursorScreenPos();
        const ImVec2 backgroundMax{ backgroundMin.x + backgroundSize.x, backgroundMin.y + backgroundSize.y };
        FLK::Dummy(backgroundSize);

        ImVec4 hudColor = ToColor(kHUDBackground);
        hudColor.w *= a_alpha;
        FLK::DrawRectFilled(backgroundMin, backgroundMax, hudColor, 14.0F);
        FLK::SetCursorPos(padding);
        hudColor = ToColor(kHUDShadow);
        hudColor.w *= a_alpha;
        FLK::PushStyleColor(ImGuiCol_TextShadow, hudColor);
        for (size_t index = 0; index < a_message.segments.size(); ++index) {
            if (index > 0) {
                FLK::SameLine(0.0F, 0.0F);
            }
            const auto& segment = a_message.segments[index];
            auto color = ToColor(HUDColors[ToIndex(segment.color)]);
            color.w *= a_alpha;
            FLK::TextColored(color, "%s", segment.text.c_str());
        }
        FLK::PopStyleColor();
        FLK::PopFont();
    }

    void DrawFLICKHUDWarning(const char* a_detailKey, const HUDOptions& a_options, float a_alpha)
    {
        const auto screen = FLK::GetDisplaySize();
        const float availableWidth = screen.x - 2.0F * HUDMargin - 2.0F * HUDWarningHorizontalPadding - HUDWarningAccentWidth;
        if (a_alpha <= 0.0F || availableWidth <= 0.0F || screen.y <= 0.0F) {
            return;
        }

        const auto title = Trans::Tr("HUD.Warning.Title");
        const auto detail = Trans::Tr(a_detailKey);
        FLK::PushFontScaled(nullptr, static_cast<float>(a_options.fontScale) / 100.0F * HUDWarningScale);
        const float textWidth = std::max(FLK::CalcTextSize(title.c_str()).x, FLK::CalcTextSize(detail.c_str()).x);
        const float lineHeight = FLK::GetTextLineHeight() * std::min(1.0F, availableWidth / std::max(1.0F, textWidth));
        FLK::PopFont();
        FLK::PushFont(FLK::GetFont(FLK::Font::kRegular), lineHeight);

        const ImVec2 cardSize{ std::min(textWidth, availableWidth) + 2.0F * HUDWarningHorizontalPadding + HUDWarningAccentWidth, 2.0F * lineHeight + HUDWarningLineGap + 2.0F * HUDWarningVerticalPadding };
        FLK::SetWindowPos(ImVec2{ (screen.x - cardSize.x) * 0.5F, (screen.y - cardSize.y) * 0.5F });
        FLK::SetCursorPos(ImVec2{ 0.0F, 0.0F });
        const auto cardMin = FLK::GetCursorScreenPos();
        const ImVec2 cardMax{ cardMin.x + cardSize.x, cardMin.y + cardSize.y };
        FLK::Dummy(cardSize);

        auto background = ToColor(kHUDBackground);
        background.w = 0.86F * a_alpha;
        auto accent = ToColor(HUDColors[ToIndex(HUDColor::Warning)]);
        accent.w *= a_alpha;
        auto text = ToColor(HUDColors[ToIndex(HUDColor::Primary)]);
        text.w *= a_alpha;
        FLK::DrawRectFilled(cardMin, cardMax, background, 5.0F);
        FLK::DrawRectFilled(cardMin, ImVec2{ cardMin.x + HUDWarningAccentWidth, cardMax.y }, accent, 5.0F);
        auto shadow = ToColor(kHUDShadow);
        shadow.w *= a_alpha;
        FLK::PushStyleColor(ImGuiCol_TextShadow, shadow);
        FLK::SetCursorPos(ImVec2{ HUDWarningAccentWidth + HUDWarningHorizontalPadding, HUDWarningVerticalPadding });
        FLK::TextColored(accent, "%s", title.c_str());
        FLK::SetCursorPos(ImVec2{ HUDWarningAccentWidth + HUDWarningHorizontalPadding, HUDWarningVerticalPadding + lineHeight + HUDWarningLineGap });
        FLK::TextColored(text, "%s", detail.c_str());
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
