#pragma once

#include "menu/hud_defs.hpp"

#define FOREACH_NOTIFICATION_TOGGLE_SETTING(SETTING) \
    /* Shows backup and restore notifications through SKSE Menu Framework. */ \
    SETTING(bool, notifications, true) \
    /* Shows each MCM completed during auto backup or restore. */ \
    SETTING(bool, perModNotificationsAuto, false) \
    /* Shows each MCM completed during manual backup or restore. */ \
    SETTING(bool, perModNotificationsManual, false)

#define FOREACH_SETTING(SETTING) \
    /* Controls how often backup checks a script that has not finished updating its buffers. */ \
    SETTING(float, actionTrialDelaySeconds, 0.5F) \
    /* Times out each awaited Papyrus call before recovery starts. */ \
    SETTING(float, scriptCallTimeoutSeconds, 30.0F) \
    FOREACH_HUD_SETTING(SETTING) \
    /* Keeps the profile updated when an MCM setting changes. */ \
    SETTING(bool, autoBackup, true) \
    /* Restores the profile after a new game's MCM registration finishes. */ \
    SETTING(bool, autoRestore, true) \
    FOREACH_NOTIFICATION_TOGGLE_SETTING(SETTING) \
    /* Lets COC start a temporary game session when testing from the main menu. */ \
    SETTING(bool, allowCOCForTesting, false) \
    /* Makes one matching MCM call appear unresponsive for testing recovery. */ \
    SETTING(std::string, testUnresponsiveMCM, std::string{}) \
    /* Controls whether Capture.json includes extra MCM menu data for debugging. */ \
    SETTING(bool, captureRawRecords, false)
