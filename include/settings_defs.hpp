#pragma once

#define FOREACH_SETTING(SETTING) \
    /* Gives MCM scripts time to process one restore call before receiving the next one. */ \
    SETTING(float, actionDelaySeconds, 0.5F) \
    /* Enables automatic restoration. */ \
    SETTING(bool, enabled, true) \
    /* Controls whether Capture.json includes extra MCM menu data for debugging. */ \
    SETTING(bool, captureRawRecords, false)
