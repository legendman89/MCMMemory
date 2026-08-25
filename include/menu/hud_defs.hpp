#pragma once

#define FOREACH_HUD_COLOR(HUD_COLOR) \
    HUD_COLOR(Primary, 0.08F, 0.09F, 0.11F, 1.00F) \
    HUD_COLOR(Success, 0.12F, 0.36F, 0.18F, 1.00F) \
    HUD_COLOR(Accent, 0.46F, 0.30F, 0.06F, 1.00F) \
    HUD_COLOR(Warning, 0.68F, 0.28F, 0.04F, 1.00F) \
    HUD_COLOR(Error, 0.66F, 0.10F, 0.12F, 1.00F) \
    HUD_COLOR(Muted, 0.28F, 0.30F, 0.34F, 1.00F)

#define FOREACH_HUD_FONT_SETTING(HUD_SETTING) \
    /* Setting type, stored name, default, HUD name, minimum, maximum, menu label and display format. */ \
    HUD_SETTING(int, notificationFontScale, 125, fontScale, 50, 200, "Font scale", "%d%%")

#define FOREACH_HUD_OFFSET_SETTING(HUD_SETTING) \
    HUD_SETTING(int, notificationHorizontalOffset, 30, horizontalOffset, 0, 4000, "Horizontal offset", "%d px") \
    HUD_SETTING(int, notificationVerticalOffset, 30, verticalOffset, 0, 4000, "Vertical offset", "%d px")

#define FOREACH_HUD_APPEARANCE_SETTING(HUD_SETTING) \
    FOREACH_HUD_FONT_SETTING(HUD_SETTING) \
    FOREACH_HUD_OFFSET_SETTING(HUD_SETTING)

#define FOREACH_HUD_TIMING_SETTING(HUD_SETTING) \
    HUD_SETTING(float, notificationStartDelaySeconds, 2.0F, startDelaySeconds, 0.0F, 10.0F, "Start delay", "%.1f s") \
    HUD_SETTING(float, notificationOperationDelaySeconds, 2.0F, operationDelaySeconds, 0.0F, 10.0F, "Operation delay", "%.1f s") \
    HUD_SETTING(float, notificationMenuCloseDelaySeconds, 2.0F, menuCloseDelaySeconds, 0.0F, 10.0F, "Menu close delay", "%.1f s") \
    HUD_SETTING(float, notificationSummaryDelaySeconds, 2.0F, summaryDelaySeconds, 0.0F, 10.0F, "Summary delay", "%.1f s") \
    HUD_SETTING(float, notificationGapSeconds, 0.5F, gapSeconds, 0.0F, 5.0F, "Message gap", "%.1f s") \
    HUD_SETTING(float, notificationDurationSeconds, 4.0F, durationSeconds, 0.5F, 10.0F, "Display duration", "%.1f s") \
    HUD_SETTING(float, notificationFadeSeconds, 1.0F, fadeSeconds, 0.0F, 5.0F, "Fade duration", "%.1f s")

#define FOREACH_HUD_WARNING_SETTING(HUD_SETTING) \
    HUD_SETTING(float, restoreWarningDurationSeconds, 5.0F, warningDurationSeconds, 1.0F, 10.0F, "Warning duration", "%.1f s")

#define FOREACH_HUD_SETTING(HUD_SETTING) \
    FOREACH_HUD_APPEARANCE_SETTING(HUD_SETTING) \
    FOREACH_HUD_TIMING_SETTING(HUD_SETTING) \
    FOREACH_HUD_WARNING_SETTING(HUD_SETTING)
