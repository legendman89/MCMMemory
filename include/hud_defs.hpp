#pragma once

#define FOREACH_HUD_COLOR(HUD_COLOR) \
    HUD_COLOR(Primary, 0.95F, 0.96F, 0.98F, 1.00F) \
    HUD_COLOR(Success, 0.30F, 0.88F, 0.42F, 1.00F) \
    HUD_COLOR(Accent, 0.92F, 0.70F, 0.30F, 1.00F) \
    HUD_COLOR(Warning, 1.00F, 0.48F, 0.14F, 1.00F) \
    HUD_COLOR(Error, 0.95F, 0.28F, 0.28F, 1.00F) \
    HUD_COLOR(Muted, 0.68F, 0.70F, 0.76F, 1.00F)

#define FOREACH_HUD_APPEARANCE_SETTING(HUD_SETTING) \
    /* Setting type, stored name, default, HUD name, minimum, maximum, menu label and display format. */ \
    HUD_SETTING(int, notificationFontScale, 125, fontScale, 50, 200, "Font scale", "%d%%")

#define FOREACH_HUD_TIMING_SETTING(HUD_SETTING) \
    HUD_SETTING(float, notificationStartDelaySeconds, 2.0F, startDelaySeconds, 0.0F, 15.0F, "Operation start delay", "%.1f s") \
    HUD_SETTING(float, notificationMenuCloseDelaySeconds, 2.0F, menuCloseDelaySeconds, 0.0F, 15.0F, "Menu close delay", "%.1f s") \
    HUD_SETTING(float, notificationSummaryDelaySeconds, 2.0F, summaryDelaySeconds, 0.0F, 15.0F, "Summary delay", "%.1f s") \
    HUD_SETTING(float, notificationGapSeconds, 0.5F, gapSeconds, 0.0F, 5.0F, "Message gap", "%.1f s") \
    HUD_SETTING(float, notificationDurationSeconds, 4.0F, durationSeconds, 0.5F, 15.0F, "Display duration", "%.1f s") \
    HUD_SETTING(float, notificationFadeSeconds, 1.0F, fadeSeconds, 0.0F, 5.0F, "Fade duration", "%.1f s")

#define FOREACH_HUD_SETTING(HUD_SETTING) \
    FOREACH_HUD_APPEARANCE_SETTING(HUD_SETTING) \
    FOREACH_HUD_TIMING_SETTING(HUD_SETTING)
