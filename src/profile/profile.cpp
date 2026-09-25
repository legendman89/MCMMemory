
#include "utils/json.hpp"
#include "utils/helper.hpp"
#include "profile/action.hpp"
#include "profile/profile.hpp"
#include "profile/profiles.hpp"
#include "profile/profile_defs.hpp"
#include "mcm/mcm_support.hpp"

#include "settings.hpp"

#include <climits>

namespace MCMMemory
{

    bool ProfileStorage::Load(Profile& a_profile)
    {
        return Load(GetSettings().activeProfile, a_profile);
    }

    bool ProfileStorage::Load(std::string_view a_name, Profile& a_profile)
    {
        std::scoped_lock lock(profileMutex);
        const auto found = pendingProfiles.find(a_name);
        if (found != pendingProfiles.end()) {
            a_profile = found->second;
            return true;
        }
        return LoadFile(a_name, a_profile);
    }

    bool ProfileStorage::LoadFile(std::string_view a_name, Profile& a_profile)
    {
        a_profile.Clear();
        const auto path = Path(a_name);
        std::ifstream stream(path);
        if (!stream) {
            return false;
        }

        try {
            auto document = JSON::DecodeDocumentText(nlohmann::json::parse(stream));
            if (!document.is_object()) {
                logger::error("Profile root is not an object in {}", ToUTF8(path));
                return false;
            }
            auto mods = document.find("mods");
            if (mods != document.end() && mods->is_object()) {
                for (const auto& item : mods->items()) {
                    if (item.key().empty() || !item.value().is_object()) {
                        continue;
                    }
                    // ReadValue would throw on a non-string mode and make the whole profile unreadable.
                    const auto mode = JSON::ReadString(item.value(), "mode").value_or(std::string{});
                    const auto parsed = ParseProfileMode(mode);
                    if (parsed == ProfileMode::Value && mode != ProfileModeName(ProfileMode::Value)) {
                        logger::warn("Profile mod '{}' has an unknown mode '{}' and is read as '{}'", item.key(), mode, ProfileModeName(ProfileMode::Value));
                    }
                    a_profile.SetMode(item.key(), parsed);
                }
            }

            const auto exclusions = document.find("pageExclusions");
            if (exclusions != document.end() && exclusions->is_object()) {

                for (const auto& item : exclusions->items()) {

                    if (item.key().empty() || !item.value().is_array()) {
                        continue;
                    }

                    for (const auto& entry : item.value()) {
                        // Invalid optional page entries must not prevent settings from loading.
                        if (!entry.is_object()) {
                            continue;
                        }

                        const auto name = JSON::ReadString(entry, "pageName");
                        const auto index = entry.find("pageIndex");
                        const auto mode = ParsePageExclusionMode(JSON::ReadString(entry, "mode").value_or(""));
                        
                        // Validate the page index and mode.
                        if (!name || index == entry.end() || !index->is_number_integer() || mode == PageExclusionMode::Include) {
                            continue;
                        }
                        if (index->is_number_unsigned() && index->get<uint64_t>() > INT_MAX) {
                            continue;
                        }
                        const auto pageIndex = index->get<int64_t>();
                        if (pageIndex < -1 || pageIndex > INT_MAX) {
                            continue;
                        }

                        MCMPageExclusion page;
                        page.name = *name;
                        page.index = static_cast<int>(pageIndex);
                        page.mode = mode;
                        const auto matchIndex = entry.find("matchIndex");
                        page.matchIndex = matchIndex != entry.end() && matchIndex->is_boolean() && matchIndex->get<bool>();
                        a_profile.pageExclusions[item.key()].push_back(std::move(page));
                    }
                }

            }

            auto settings = document.find("settings");
            if (settings == document.end() || !settings->is_array()) {
                logger::error("Profile settings are missing or invalid in {}", ToUTF8(path));
                return false;
            }
            for (const auto& settingDocument : *settings) {
                CapturedSetting setting;
                if (FromJson(settingDocument, setting) && setting.identityComplete) {
                    a_profile.settings.push_back(std::move(setting));
                }
            }
            auto activations = document.find("activations");
            if (activations != document.end() && activations->is_array()) {
                for (const auto& activationDocument : *activations) {
                    if (!activationDocument.is_object()) {
                        continue;
                    }
                    MCMActivation activation;
#define READ_ACTIVATION_FIELD(object, key, member) JSON::ReadValue(activationDocument, key, object.member);
                    FOREACH_ACTIVATION_FIELD(READ_ACTIVATION_FIELD, activation)
#undef READ_ACTIVATION_FIELD
                    JSON::ReadValue(activationDocument, "startCommand", activation.startCommand);
                    std::string controlType;
                    JSON::ReadValue(activationDocument, "controlType", controlType);
                    activation.type = controlType.empty() ? ControlType::Unknown : ParseControlType(controlType);
                    const bool validValue = activation.type == ControlType::Option || activation.startCommand || !activation.enabledText.empty();
                    if (activation.selection.identity.modID.empty() || activation.selection.pageIndex < -1 || activation.selection.optionIndex < 0 || activation.optionLabel.empty() || !validValue) {
                        continue;
                    }
                    if (activation.startCommand && !MCMActivationSupport::IsStoredCommandValid(activation)) {
                        logger::warn("Skipped activation control '{}' from '{}': it no longer reads as an enable command", activation.optionLabel, activation.selection.identity.modID);
                        continue;
                    }
                    a_profile.SetActivation(activation);
                }
            }
        } catch (const std::exception& error) {
            logger::error("Failed to read profile {}: {}", ToUTF8(path), error.what());
            return false;
        }

        pageExclusionCache[std::string(a_name)] = a_profile.pageExclusions;

        return true;
    }

    bool ProfileStorage::ForgetMCMs(std::string_view a_name, const MCMFilter& a_modIDs, size_t& a_settingCount)
    {
        a_settingCount = 0;
        if (a_modIDs.empty() || !Profiles::IsValidName(a_name)) {
            return false;
        }

        std::scoped_lock lock(profileMutex);
        Profile profile;
        const auto pending = pendingProfiles.find(a_name);
        if (pending != pendingProfiles.end()) {
            profile = pending->second;
        }
        else if (!LoadFile(a_name, profile)) {
            logger::error("Refusing to edit an unreadable persistent profile at {}", ToUTF8(Path(a_name)));
            return false;
        }

        auto setting = profile.settings.begin();
        while (setting != profile.settings.end()) {
            if (ContainsMCMID(a_modIDs, setting->selection.identity.modID)) {
                setting = profile.settings.erase(setting);
                ++a_settingCount;
            }
            else {
                ++setting;
            }
        }

        auto activation = profile.activations.begin();
        while (activation != profile.activations.end()) {
            if (ContainsMCMID(a_modIDs, activation->selection.identity.modID)) {
                activation = profile.activations.erase(activation);
            }
            else {
                ++activation;
            }
        }

        // A forgotten MCM goes back to the default mode, so recording starts over if it is used again.
        for (const auto& modID : a_modIDs) {
            profile.mods.erase(modID);
            profile.pageExclusions.erase(modID);
        }

        if (!SaveFile(a_name, profile)) {
            a_settingCount = 0;
            return false;
        }
        if (pending != pendingProfiles.end()) {
            pendingProfiles.erase(pending);
        }
        return true;
    }

    Profile* ProfileStorage::GetPendingProfile(std::string_view a_name)
    {
        if (!Profiles::IsValidName(a_name)) {
            return nullptr;
        }
        const auto found = pendingProfiles.find(a_name);
        if (found != pendingProfiles.end()) {
            return std::addressof(found->second);
        }

        Profile profile;
        std::error_code error;
        const bool exists = std::filesystem::exists(Path(a_name), error);
        if (error || (exists && !LoadFile(a_name, profile))) {
            logger::error("Refusing to overwrite an unreadable persistent profile at {}", ToUTF8(Path(a_name)));
            return nullptr;
        }
        auto inserted = pendingProfiles.emplace(std::string(a_name), std::move(profile));
        logger::debug("Loaded profile '{}' into memory for automatic backup", a_name);
        return std::addressof(inserted.first->second);
    }

    bool ProfileStorage::UpdateSetting(std::string_view a_name, const CapturedSetting& a_setting)
    {
        if (!a_setting.identityComplete) {
            return false;
        }

        std::scoped_lock lock(profileMutex);
        auto* pending = GetPendingProfile(a_name);
        if (!pending) {
            return false;
        }
        auto& profile = *pending;

        if (profile.IsPageExcluded(a_setting.selection, PageExclusionMode::BackupCapture)) {
            return true;
        }

        const auto& modID = a_setting.selection.identity.modID;
        if (GetSettings().recordActions && !profile.IsActionMode(modID)) {
            profile.SetMode(modID, ProfileMode::Action);
            // Scanned settings stay unordered until the player changes their controls.
            logger::info("Automatic backup started recording actions for '{}'", modID);
        }

        if (profile.IsActionMode(modID)) {
            AppendAction(profile.settings, a_setting);
        }
        else {
            Deduplicate(profile.settings, a_setting);
        }

        return true;
    }

    bool ProfileStorage::UpdateActivation(std::string_view a_name, const MCMActivation& a_activation, bool a_enabled)
    {
        std::scoped_lock lock(profileMutex);
        auto* pending = GetPendingProfile(a_name);
        if (!pending) {
            return false;
        }
        auto& profile = *pending;

        if (profile.IsPageExcluded(a_activation.selection, PageExclusionMode::BackupCapture)) {
            return true;
        }
        if (a_enabled) {
            profile.SetActivation(a_activation);
        }
        else {
            profile.RemoveActivation(a_activation.selection.identity.modID);
        }
        return true;
    }

    bool ProfileStorage::IsPageCaptureExcluded(std::string_view a_name, const MCMSelection& a_selection)
    {
        std::scoped_lock lock(profileMutex);
        const auto pending = pendingProfiles.find(a_name);
        if (pending != pendingProfiles.end()) {
            return pending->second.IsPageExcluded(a_selection, PageExclusionMode::BackupCapture);
        }

        auto cached = pageExclusionCache.find(a_name);
        if (cached == pageExclusionCache.end()) {
            Profile profile;
            if (!LoadFile(a_name, profile)) {
                // A missing profile has no exclusions.
                std::error_code error;
                if (std::filesystem::exists(Path(a_name), error) || error) {
                    return true;
                }
                pageExclusionCache.emplace(std::string(a_name), PageExclusionMap{});
            }
            cached = pageExclusionCache.find(a_name);
        }

        return MCMMemory::IsPageExcluded(cached->second, a_selection.identity.modID, a_selection.pageName, a_selection.pageIndex, PageExclusionMode::BackupCapture);
    }

    bool ProfileStorage::SavePageExclusions(std::string_view a_name, std::string_view a_modID, const std::vector<MCMPageExclusion>& a_pages)
    {
        if (a_modID.empty()) {
            return false;
        }

        std::scoped_lock lock(profileMutex);
        auto* pending = GetPendingProfile(a_name);
        if (!pending) {
            return false;
        }

        Profile profile = *pending;
        auto& exclusions = profile.pageExclusions[std::string(a_modID)];
        exclusions.clear();
        for (const auto& page : a_pages) {
            if (page.mode != PageExclusionMode::Include && ToIndex(page.mode) < pageExclusionModeNames.size()) {
                exclusions.push_back(page);
            }
        }

        if (exclusions.empty()) {
            profile.pageExclusions.erase(std::string(a_modID));
        }

        if (!SaveFile(a_name, profile)) {
            return false;
        }

        pendingProfiles.erase(std::string(a_name));
        
        return true;
    }

    bool ProfileStorage::FlushPending()
    {
        std::scoped_lock lock(profileMutex);
        bool saved = true;
        auto pending = pendingProfiles.begin();
        while (pending != pendingProfiles.end()) {
            if (SaveFile(pending->first, pending->second)) {
                pending = pendingProfiles.erase(pending);
            }
            else {
                logger::error("Could not save profile '{}'; captured changes remain in memory for retry", pending->first);
                saved = false;
                ++pending;
            }
        }
        return saved;
    }

    bool ProfileStorage::Save(const Profile& a_profile)
    {
        return Save(GetSettings().activeProfile, a_profile);
    }

    bool ProfileStorage::Save(std::string_view a_name, const Profile& a_profile)
    {
        std::scoped_lock lock(profileMutex);
        if (!SaveFile(a_name, a_profile)) {
            return false;
        }
        const auto pending = pendingProfiles.find(a_name);
        if (pending != pendingProfiles.end()) {
            pendingProfiles.erase(pending);
        }
        return true;
    }

    bool ProfileStorage::SaveFile(std::string_view a_name, const Profile& a_profile)
    {
        const auto path = Path(a_name);
        if (!JSON::WriteFile(path, ToJson(a_profile))) {
            return false;
        }
        pageExclusionCache[std::string(a_name)] = a_profile.pageExclusions;
        logger::info("Saved {} persistent profile settings to {}", a_profile.settings.size(), ToUTF8(path));
        return true;
    }

    std::filesystem::path ProfileStorage::Path()
    {
        return Path(GetSettings().activeProfile);
    }

    std::filesystem::path ProfileStorage::Path(std::string_view a_name)
    {
        const auto name = Profiles::IsValidName(a_name) ? a_name : std::string_view{ "Default" };
        return Profiles::Directory() / FromUTF8(std::format("{}.json", name));
    }

    bool ProfileStorage::FromJson(const nlohmann::json& a_document, CapturedSetting& a_setting)
    {
        if (!a_document.is_object()) {
            return false;
        }

        std::string controlType;
        JSON::ReadValue(a_document, "sourceEventID", a_setting.sourceEventID);
        JSON::ReadValue(a_document, "controlType", controlType);
        JSON::ReadValue(a_document, "modIndex", a_setting.selection.modIndex);
#define READ_SELECTION_FIELD(object, key, member) JSON::ReadValue(a_document, key, object.member);
        FOREACH_PROFILE_SELECTION_FIELD(READ_SELECTION_FIELD, a_setting.selection)
#undef READ_SELECTION_FIELD
        JSON::ReadValue(a_document, "optionLabel", a_setting.optionLabel);
        JSON::ReadValue(a_document, "rowLabel", a_setting.rowLabel.label);
        JSON::ReadValue(a_document, "rowDistance", a_setting.rowLabel.distance);
        JSON::ReadValue(a_document, "settingID", a_setting.settingID);
        JSON::ReadValue(a_document, "stateName", a_setting.stateName);
#define READ_SETTING_FLAG(object, key, member) JSON::ReadValue(a_document, key, object.member);
        FOREACH_SETTING_FLAG(READ_SETTING_FLAG, a_setting)
#undef READ_SETTING_FLAG
        JSON::ReadValue(a_document, "command", a_setting.command);
        JSON::ReadValue(a_document, "confirmedCommand", a_setting.confirmedCommand);
        JSON::ReadValue(a_document, "sequence", a_setting.sequence);
        // ReadString ignores a wrongly typed field instead of throwing the whole profile away.
        if (auto valueText = JSON::ReadString(a_document, "valueText")) {
            a_setting.valueText = std::move(*valueText);
        }
        JSON::ReadValue(a_document, "valueSource", a_setting.valueSource);
        JSON::ReadValue(a_document, "identityComplete", a_setting.identityComplete);
        JSON::ReadValue(a_document, "value", a_setting.value);
        
        a_setting.type = ParseControlType(controlType);

        return (a_setting.type != ControlType::Unknown || (a_setting.command && a_setting.recorded)) && !a_setting.selection.identity.modID.empty() && a_setting.selection.optionIndex >= 0 && !a_setting.value.is_null();
    }

    nlohmann::ordered_json ProfileStorage::ToJson(const Profile& a_profile)
    {
        // Keep the profile description before its saved MCM data.
        nlohmann::ordered_json document;
        document["formatVersion"] = 2;
        document["purpose"] = "Persistent MCM settings profile";

        // Value is the default, so only mods that record actions are written here.
        nlohmann::json mods = nlohmann::json::object();
        for (const auto& [modID, mode] : a_profile.mods) {
            if (mode == ProfileMode::Value) {
                continue;
            }
            nlohmann::json modDocument;
            modDocument["mode"] = std::string(ProfileModeName(mode));
            mods[modID] = std::move(modDocument);
        }

        if (!mods.empty()) {
            document["mods"] = std::move(mods);
        }

        // Save page exclusions for mods that have them, but skip any that are Include or invalid.
        if (!a_profile.pageExclusions.empty()) {
            nlohmann::json exclusions = nlohmann::json::object();
            for (const auto& [modID, pages] : a_profile.pageExclusions) {
                for (const auto& page : pages) {
                    if (page.mode == PageExclusionMode::Include || ToIndex(page.mode) >= pageExclusionModeNames.size()) {
                        continue;
                    }
                    nlohmann::json entry{ { "pageName", page.name }, { "pageIndex", page.index }, { "mode", pageExclusionModeNames[ToIndex(page.mode)] } };
                    if (page.matchIndex) {
                        entry["matchIndex"] = true;
                    }
                    exclusions[modID].push_back(std::move(entry));
                }
            }
            if (!exclusions.empty()) {
                document["pageExclusions"] = std::move(exclusions);
            }
        }

        // Save activations.
        if (!a_profile.activations.empty()) {
            document["activations"] = nlohmann::json::array();
            for (const auto& activation : a_profile.activations) {
                nlohmann::json activationDocument;
#define WRITE_ACTIVATION_FIELD(object, key, member) activationDocument[key] = object.member;
                FOREACH_ACTIVATION_FIELD(WRITE_ACTIVATION_FIELD, activation)
#undef WRITE_ACTIVATION_FIELD
                if (activation.startCommand) {
                    activationDocument["startCommand"] = true;
                }
                activationDocument["controlType"] = std::string(ControlTypeName(activation.type));
                document["activations"].push_back(std::move(activationDocument));
            }
        }

        // Save settings.
        document["settings"] = nlohmann::json::array();
        for (const auto& setting : a_profile.settings) {
            document["settings"].push_back(JSON::ToJson(setting, false));
        }

        return document;
    }
}
