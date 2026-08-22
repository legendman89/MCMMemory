#pragma once

#define FOREACH_SETTING(SETTING) \
    /* Controls how often backup checks a script that has not finished updating its buffers. */ \
    SETTING(float, actionTrialDelaySeconds, 0.5F) \
    /* Keeps the profile updated and creates the first full backup when needed. */ \
    SETTING(bool, autoBackup, true) \
    /* Restores the profile after MCM registration finishes. */ \
    SETTING(bool, autoRestore, true) \
    /* Shows operation summaries through SKSE Menu Framework. */ \
    SETTING(bool, notifications, true) \
    /* Controls whether Capture.json includes extra MCM menu data for debugging. */ \
    SETTING(bool, captureRawRecords, false)
