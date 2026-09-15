#pragma once

#define FOREACH_PROFILE_SELECTION_FIELD(FIELD, object) \
    FIELD(object, "modName", identity.modName) \
    FIELD(object, "modID", identity.modID) \
    FIELD(object, "pageName", pageName) \
    FIELD(object, "pageIndex", pageIndex) \
    FIELD(object, "optionIndex", optionIndex)

#define FOREACH_ACTIVATION_FIELD(FIELD, object) \
    FOREACH_PROFILE_SELECTION_FIELD(FIELD, object.selection) \
    FIELD(object, "optionLabel", optionLabel) \
    FIELD(object, "stateName", stateName) \
    FIELD(object, "enabledText", enabledText)

// These flags are excluded from JSON when false.
#define FOREACH_SETTING_FLAG(FIELD, object) \
    FIELD(object, "pageScopedState", pageScopedState) \
    FIELD(object, "textControl", textControl) \
    FIELD(object, "recorded", recorded) \
    FIELD(object, "rebuildsPage", rebuildsPage) \
    FIELD(object, "reopensConfig", reopensConfig)
