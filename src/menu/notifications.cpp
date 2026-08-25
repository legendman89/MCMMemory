#include "menu/notifications.hpp"

#include "menu/hud.hpp"
#include "menu/icons.hpp"
#include "menu/translate.hpp"
#include "settings.hpp"

namespace MCMMemory::Menu
{
    void __stdcall RenderNotifications()
    {
        auto& settings = GetSettings();
        constexpr float hudSliderWidth = 360.0F;
        bool settingsChanged{};
        bool appearanceSettingActive{};

        if (GUI::Checkbox(Trans::Tr("Notifications.Enable").c_str(), std::addressof(settings.notifications))) {
            settingsChanged = true;
        }

        GUI::BeginDisabled(!settings.notifications);

        if (GUI::Checkbox(Trans::Tr("Notifications.PerMod.Automatic").c_str(), std::addressof(settings.perModNotificationsAuto))) {
            settingsChanged = true;
        }

        GUI::SameLine(0.0F, 18.0F);

        if (GUI::Checkbox(Trans::Tr("Notifications.PerMod.Manual").c_str(), std::addressof(settings.perModNotificationsManual))) {
            settingsChanged = true;
        }

        GUI::SeparatorText(Trans::Tr("Notifications.Appearance.Header").c_str());

        if (IconButton(Trans::Tr("Notifications.Appearance.Preview").c_str(), Icons::kPreview, Color::kPreviewButtonColors)) {
            HUD::GetSingleton()->Preview();
        }

        GUI::Spacing();

#define DRAW_HUD_FONT_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
        GUI::SetNextItemWidth(hudSliderWidth); \
        if (GUI::SliderInt(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
            settingsChanged = true; \
        } \
        appearanceSettingActive = appearanceSettingActive || GUI::IsItemActive();
        FOREACH_HUD_FONT_SETTING(DRAW_HUD_FONT_SETTING)
#undef DRAW_HUD_FONT_SETTING

        const auto compactTableFlags = GUI::ImGuiTableFlags_SizingStretchSame;
        if (GUI::BeginTable("HUD notification offsets", 2, compactTableFlags)) {
#define DRAW_HUD_OFFSET_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
            GUI::TableNextColumn(); \
            GUI::SetNextItemWidth(hudSliderWidth); \
            if (GUI::SliderInt(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                settingsChanged = true; \
            } \
            appearanceSettingActive = appearanceSettingActive || GUI::IsItemActive();
            FOREACH_HUD_OFFSET_SETTING(DRAW_HUD_OFFSET_SETTING)
#undef DRAW_HUD_OFFSET_SETTING
            GUI::EndTable();
        }

        GUI::SeparatorText(Trans::Tr("Notifications.Timing.Header").c_str());
        if (GUI::BeginTable("HUD notification timing", 2, compactTableFlags)) {
#define DRAW_HUD_TIMING_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
            GUI::TableNextColumn(); \
            GUI::SetNextItemWidth(hudSliderWidth); \
            if (GUI::SliderFloat(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                settingsChanged = true; \
            }
            FOREACH_HUD_TIMING_SETTING(DRAW_HUD_TIMING_SETTING)
#undef DRAW_HUD_TIMING_SETTING
            GUI::EndTable();
        }
        GUI::EndDisabled();

        GUI::SeparatorText(Trans::Tr("Notifications.Warning.Header").c_str());

        // The restore warning remains available when normal notifications are disabled.
#define DRAW_HUD_WARNING_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
        GUI::SetNextItemWidth(hudSliderWidth); \
        if (GUI::SliderFloat(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
            settingsChanged = true; \
        }
        FOREACH_HUD_WARNING_SETTING(DRAW_HUD_WARNING_SETTING)
#undef DRAW_HUD_WARNING_SETTING

        if (settingsChanged) {
            HUD::GetSingleton()->Configure(settings);
            if (!SettingsStorage::Save()) {
                logger::error("MCM Memory menu could not save its notification settings");
            }
        }

        if (appearanceSettingActive) {
            HUD::GetSingleton()->KeepPreviewAlive();
        }
    }
}
