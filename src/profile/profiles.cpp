#include "profile/profiles.hpp"

#include "settings.hpp"
#include "utils/helper.hpp"

namespace MCMMemory
{
    std::vector<std::string> Profiles::ReadNames()
    {
        std::vector<std::string> names;
        std::error_code error;
        if (std::filesystem::exists(Directory(), error)) {
            for (std::filesystem::directory_iterator entry(Directory(), error), end; !error && entry != end; entry.increment(error)) {
                const auto extension = ToUTF8(entry->path().extension());
                const auto name = ToUTF8(entry->path().stem());
                if (entry->is_regular_file(error) && !error && ContainsCaseInsensitive(extension, ".json") && IsValidName(name)) {
                    names.push_back(name);
                }
            }
        }
        if (error) {
            logger::error("Failed to read profile directory {}: {}", ToUTF8(Directory()), error.message());
        }

        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        if (std::find(names.begin(), names.end(), "Default") == names.end()) {
            names.insert(names.begin(), "Default");
        }
        return names;
    }

    bool Profiles::Create(std::string_view a_name, std::string_view a_source, std::string& a_error)
    {
        a_error.clear();
        if (!IsValidName(a_name)) {
            a_error = "Profile.Error.InvalidName";
            return false;
        }

        std::error_code error;
        if (std::filesystem::exists(ProfileStorage::Path(a_name), error)) {
            a_error = "Profile.Error.DuplicateName";
            return false;
        }
        if (error) {
            a_error = "Profile.Error.LocationCheck";
            return false;
        }

        Profile profile;
        if (!a_source.empty() && !ProfileStorage::Load(a_source, profile)) {
            a_error = "Profile.Error.DuplicateFailed";
            return false;
        }
        if (!ProfileStorage::Save(a_name, profile)) {
            a_error = "Profile.Error.CreateFailed";
            return false;
        }
        if (!Select(a_name, a_error)) {
            std::error_code cleanupError;
            std::filesystem::remove(ProfileStorage::Path(a_name), cleanupError);
            return false;
        }

        if (a_source.empty()) {
            logger::info("Created empty profile '{}'", a_name);
        }
        else {
            logger::info("Created profile '{}' from '{}'", a_name, a_source);
        }
        return true;
    }

    bool Profiles::Delete(std::string_view a_name, std::string& a_error)
    {
        a_error.clear();
        const auto names = ReadNames();
        if (names.size() <= 1) {
            a_error = "Profile.Error.DeleteLast";
            return false;
        }

        std::error_code error;
        std::string nextName{ "Default" };
        for (const auto& profileName : names) {
            if (profileName != a_name && std::filesystem::exists(ProfileStorage::Path(profileName), error) && !error) {
                nextName = profileName;
                break;
            }
        }
        if (!Select(nextName, a_error)) {
            return false;
        }

        if (!std::filesystem::remove(ProfileStorage::Path(a_name), error)) {
            a_error = error ? "Profile.Error.DeleteFailed" : "Profile.Error.NotCreated";
            std::string rollbackError;
            Select(a_name, rollbackError);
            return false;
        }

        logger::info("Deleted profile '{}' and selected '{}'", a_name, nextName);
        return true;
    }

    bool Profiles::Select(std::string_view a_name, std::string& a_error)
    {
        a_error.clear();
        if (!IsValidName(a_name)) {
            a_error = "Profile.Error.SelectedNameInvalid";
            return false;
        }

        auto& settings = GetSettings();
        const auto previous = settings.activeProfile;
        settings.activeProfile = a_name;
        if (!SettingsStorage::Save()) {
            settings.activeProfile = previous;
            a_error = "Profile.Error.SaveSelection";
            return false;
        }

        logger::info("Selected profile '{}'", a_name);
        return true;
    }

    bool Profiles::CheckSelection()
    {
        auto& settings = GetSettings();
        const auto names = ReadNames();
        if (IsValidName(settings.activeProfile) && std::find(names.begin(), names.end(), settings.activeProfile) != names.end()) {
            return true;
        }

        logger::warn("Active profile '{}' is unavailable; selecting Default", settings.activeProfile);
        settings.activeProfile = "Default";
        return SettingsStorage::Save();
    }

    bool Profiles::IsValidName(std::string_view a_name)
    {
        if (a_name.empty() || a_name.size() > 80 || a_name == "." || a_name == ".." || a_name.back() == ' ' || a_name.back() == '.') {
            return false;
        }
        for (const unsigned char character : a_name) {
            if (character < 32 || std::string_view{ "<>:\"/\\|?*" }.contains(static_cast<char>(character))) {
                return false;
            }
        }
        return !IsReservedName(a_name);
    }

    bool Profiles::IsReservedName(std::string_view a_name)
    {
        const auto stem = a_name.substr(0, a_name.find('.'));
        std::string lower;
        lower.reserve(stem.size());
        for (const unsigned char character : stem) {
            lower.push_back(static_cast<char>(ToLowerASCII(character)));
        }
        if (lower == "con" || lower == "prn" || lower == "aux" || lower == "nul") {
            return true;
        }
        return lower.size() == 4 && (lower.starts_with("com") || lower.starts_with("lpt")) && lower[3] >= '1' && lower[3] <= '9';
    }
}
