#pragma once

#define FOREACH_SETTING(SETTING) \
    /* Controls how often backup checks a script that has not finished updating its buffers. */ \
    SETTING(float, actionTrialDelaySeconds, 0.5F) \
    /* Keeps the profile updated and creates the first full backup when needed. */ \
    SETTING(bool, autoBackup, true) \
    /* Restores the profile after MCM registration finishes. */ \
    SETTING(bool, autoRestore, true) \
    /* Shows backup and restore notifications through SKSE Menu Framework. */ \
    SETTING(bool, notifications, true) \
    /* Shows the latest MCM completed during backup or restore. */ \
    SETTING(bool, individualMCMNotifications, false) \
    /* Controls HUD notification text size as a percentage. */ \
    SETTING(int, notificationFontScale, 125) \
    /* Controls how long a HUD notification remains fully visible. */ \
    SETTING(float, notificationDurationSeconds, 4.0F) \
    /* Controls how long a HUD notification takes to fade out. */ \
    SETTING(float, notificationFadeSeconds, 1.0F) \
    /* Controls whether Capture.json includes extra MCM menu data for debugging. */ \
    SETTING(bool, captureRawRecords, false)
