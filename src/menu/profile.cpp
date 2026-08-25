#include "menu/profile.hpp"

#include "mcm/mcm_registry.hpp"
#include "menu/icons.hpp"
#include "menu/translate.hpp"
#include "profile/backup.hpp"
#include "profile/profile.hpp"
#include "profile/profiles.hpp"
#include "profile/restore.hpp"
#include "settings.hpp"
#include "utils/helper.hpp"

namespace MCMMemory::Menu
{
    inline constexpr auto registryRefreshInterval{ std::chrono::seconds(5) };
    inline constexpr auto ProfileFieldWidth{ 240.0F };

    void ProfileMenu::RefreshProfileNames()
    {
        profileNames = Profiles::ReadNames();
    }

    void ProfileMenu::RenderProfileSelector(bool a_operationRunning)
    {
        if (profileNames.empty()) {
            RefreshProfileNames();
        }

        auto& settings = GetSettings();
        GUI::BeginDisabled(a_operationRunning);
        GUI::TextUnformatted(Trans::Tr("Profile:").c_str());
        GUI::SameLine();
        GUI::SetNextItemWidth(ProfileFieldWidth);
        if (GUI::BeginCombo("##Active Profile", settings.activeProfile.c_str())) {
            for (const auto& profileName : profileNames) {
                const bool selected = profileName == settings.activeProfile;
                if (GUI::Selectable(profileName.c_str(), selected)) {
                    std::string error;
                    if (Profiles::Select(profileName, error)) {
                        loaded = false;
                    }
                    else {
                        logger::error("Profile selection failed: {}", error);
                    }
                }
            }
            GUI::EndCombo();
        }

        GUI::SameLine(0.0F, 10.0F);
        if (GUI::Button(Trans::Tr("Create").c_str())) {
            createProfileWindow = {};
            createProfileWindow.open = true;
            createProfileWindow.sourceProfile = settings.activeProfile;
        }

        std::error_code error;
        const bool selectedProfileExists = std::filesystem::exists(ProfileStorage::Path(), error) && !error;
        GUI::SameLine();
        GUI::BeginDisabled(!selectedProfileExists || profileNames.size() <= 1);
        if (GUI::Button(Trans::Tr("Delete").c_str())) {
            deleteProfileWindow = {};
            deleteProfileWindow.open = true;
            deleteProfileWindow.profile = settings.activeProfile;
        }
        GUI::EndDisabled();
        GUI::EndDisabled();
    }

    void ProfileMenu::RenderProfileControls()
    {
        const auto backupStatus = Backup::GetSingleton()->GetStatus();
        const auto restoreStatus = Restore::GetSingleton()->GetStatus();
        const bool operationRunning = backupStatus != OperationStatus::Idle || restoreStatus != OperationStatus::Idle;
        const bool profileEditing = createProfileWindow.open || deleteProfileWindow.open;
        const float rowStart = GUI::GetCursorPosX();
        const float rowWidth = GUI::GetContentRegionAvail().x;

        RenderProfileSelector(operationRunning || profileEditing);

        constexpr float operationControlsWidth{ 445.0F };
        GUI::SameLine();
        GUI::SetCursorPosX(std::max(GUI::GetCursorPosX() + 20.0F, rowStart + rowWidth - operationControlsWidth));
        RenderOperationButtons();
    }

    bool ProfileMenu::NeedsRefresh() const
    {
        if (!loaded) {
            return true;
        }
        if (IsGameLoaded() != gameLoaded) {
            return true;
        }

        std::error_code error;
        const bool exists = std::filesystem::exists(ProfileStorage::Path(), error);
        if (error || exists != profileAvailable) {
            return !error;
        }
        if (exists && std::filesystem::last_write_time(ProfileStorage::Path(), error) != profileWriteTime) {
            return !error;
        }
        return IsGameLoaded() && !registrySettled && std::chrono::steady_clock::now() >= nextRegistryRefresh;
    }

    SelectedMCMFilters ProfileMenu::ReadSelectedMCMs() const
    {
        SelectedMCMFilters selectedMCMs;
        for (const auto& mcm : mcms) {
            if (mcm.selected) {
                selectedMCMs.backup.push_back(mcm.identity.modID);
                if (mcm.settingCount > 0) {
                    selectedMCMs.restore.push_back(mcm.identity.modID);
                }
            }
        }
        return selectedMCMs;
    }

    ProfileMCMRow& ProfileMenu::FindOrAddMCM(const MCMIdentity& a_identity, const MCMFilter& a_selectedMCMs)
    {
        auto mcm = mcms.begin();
        for (; mcm != mcms.end() && mcm->identity.modID != a_identity.modID; ++mcm) {}
        if (mcm != mcms.end()) {
            return *mcm;
        }

        ProfileMCMRow row;
        row.identity = a_identity;
        row.selected = ContainsMCMID(a_selectedMCMs, row.identity.modID);
        mcms.push_back(std::move(row));
        return mcms.back();
    }

    void ProfileMenu::Refresh()
    {
        const auto selectedMCMs = ReadSelectedMCMs().backup;
        const bool currentGameLoaded = IsGameLoaded();
        if (currentGameLoaded != gameLoaded) {
            registryWait.Reset();
            registrySettled = false;
        }
        mcms.clear();

        std::error_code error;
        profileAvailable = std::filesystem::exists(ProfileStorage::Path(), error) && !error;
        Profile profile;
        if (profileAvailable && ProfileStorage::Load(profile)) {
            for (const auto& setting : profile) {
                ++FindOrAddMCM(setting.selection.identity, selectedMCMs).settingCount;
            }
        }

        if (currentGameLoaded) {
            const auto registeredMCMs = MCMRegistry().ReadRegisteredMCMs();
            const auto registryResult = registryWait.Update(registeredMCMs);
            registrySettled = registryResult == RegistryWaitResult::Ready;
            if (registryResult == RegistryWaitResult::Expired) {
                registryWait.Reset();
            }
            for (const auto& registeredMCM : registeredMCMs) {
                auto& mcm = FindOrAddMCM(registeredMCM.identity, selectedMCMs);
                mcm.identity = registeredMCM.identity;
                mcm.available = true;
            }
        }
        gameLoaded = currentGameLoaded;
        for (auto& mcm : mcms) {
            if (!mcm.available) {
                mcm.selected = false;
            }
        }

        if (profileAvailable) {
            profileWriteTime = std::filesystem::last_write_time(ProfileStorage::Path(), error);
        }
        nextRegistryRefresh = std::chrono::steady_clock::now() + registryRefreshInterval;
        loaded = true;
    }

    bool ProfileMenu::MatchesSearch(const ProfileMCMRow& a_mcm) const
    {
        const std::string_view searchText{ search.data() };
        const auto displayName = GetDisplayModName(a_mcm.identity.modName);
        return ContainsCaseInsensitive(displayName, searchText) || ContainsCaseInsensitive(a_mcm.identity.modID, searchText);
    }

    void ProfileMenu::SelectVisibleMCMs(bool a_selected)
    {
        for (auto& mcm : mcms) {
            if (!a_selected) {
                mcm.selected = false;
                continue;
            }
            if (mcm.available && IsVisible(mcm)) {
                mcm.selected = true;
            }
        }
    }

    void ProfileMenu::RenderOperationButtons()
    {
        auto* backup = Backup::GetSingleton();
        auto* restore = Restore::GetSingleton();
        const auto backupStatus = backup->GetStatus();
        const auto restoreStatus = restore->GetStatus();
        const bool operationRunning = backupStatus != OperationStatus::Idle || restoreStatus != OperationStatus::Idle;
        const bool operationAvailable = IsGameLoaded() && !operationRunning && !createProfileWindow.open && !deleteProfileWindow.open;
        const float rowStart = GUI::GetCursorPosX();
        const float rowWidth = GUI::GetContentRegionAvail().x;

        if (IconCTAButton(Trans::Tr("Back Up Now").c_str(), operationAvailable, Icons::kSave, Color::kBackupButtonColors)) {
            backup->Start();
        }

        GUI::SameLine(0.0F, 14.0F);

        if (IconCTAButton(Trans::Tr("Restore Now").c_str(), operationAvailable, Icons::kRestore, Color::kRestoreButtonColors)) {
            restore->Start();
        }

        const bool stopping = backupStatus == OperationStatus::Stopping || restoreStatus == OperationStatus::Stopping;
        const bool cancellationAvailable = backupStatus == OperationStatus::Running || restoreStatus == OperationStatus::Running;
        auto cancelText = Trans::Tr("Cancel");
        if (stopping) {
            cancelText = Trans::Tr("Stopping...");
        }
        else if (backupStatus == OperationStatus::Running) {
            cancelText = Trans::Tr("Cancel Backup");
        }
        else if (restoreStatus == OperationStatus::Running) {
            cancelText = Trans::Tr("Cancel Restore");
        }

        const std::array<std::string, 3> cancellationLabels{ Trans::Tr("Cancel Backup"), Trans::Tr("Cancel Restore"), Trans::Tr("Stopping...") };
        float cancelWidth{};
        for (const auto& label : cancellationLabels) {
            cancelWidth = std::max(cancelWidth, GUI::CalcTextSize(label.c_str()).x);
        }
        cancelWidth += 28.0F;

        GUI::SameLine();
        GUI::SetCursorPosX(std::max(GUI::GetCursorPosX(), rowStart + rowWidth - cancelWidth));
        if (CTAButton(cancelText.c_str(), cancellationAvailable, Color::kCancelButtonColors, GUI::ImVec2{ cancelWidth, 0.0F })) {
            if (backupStatus == OperationStatus::Running) {
                backup->Cancel();
            }
            else if (restoreStatus == OperationStatus::Running) {
                restore->Cancel();
            }
        }
        if (restoreStatus != OperationStatus::Idle) {
            WrappedTooltip(Trans::Tr("Settings already restored will not be reverted.").c_str());
        }
    }

    void ProfileMenu::RenderCreateProfileWindow()
    {
        auto& window = createProfileWindow;
        if (!window.open) {
            return;
        }

        bool sourceAvailable{};
        for (const auto& profileName : profileNames) {
            std::error_code error;
            if (std::filesystem::exists(ProfileStorage::Path(profileName), error) && !error) {
                sourceAvailable = true;
                if (window.sourceProfile.empty() || !std::filesystem::exists(ProfileStorage::Path(window.sourceProfile), error)) {
                    window.sourceProfile = profileName;
                }
            }
        }
        if (!sourceAvailable) {
            window.duplicate = false;
        }

        GUI::SetNextWindowSize(GUI::ImVec2{ 440.0F, 210.0F }, GUI::ImGuiCond_FirstUseEver);
        CenterNextWindow();
        const auto title = std::format("{}###Create MCM Memory Profile", Trans::Tr("Create Profile"));
        if (GUI::Begin(title.c_str(), std::addressof(window.open), GUI::ImGuiWindowFlags_NoCollapse)) {

            GUI::SetNextItemWidth(ProfileFieldWidth);
            if (GUI::InputText(Trans::Tr("Name").c_str(), window.name.data(), window.name.size())) {
                window.error.clear();
            }

            if (GUI::RadioButton(Trans::Tr("Empty profile").c_str(), !window.duplicate)) {
                window.duplicate = false;
            }

            GUI::SameLine(0.0F, 20.0F);

            GUI::BeginDisabled(!sourceAvailable);

            if (GUI::RadioButton(Trans::Tr("Duplicate profile").c_str(), window.duplicate)) {
                window.duplicate = true;
            }
            GUI::EndDisabled();

            if (window.duplicate) {

                GUI::SetNextItemWidth(ProfileFieldWidth);
                if (GUI::BeginCombo(Trans::Tr("Copy from").c_str(), window.sourceProfile.c_str())) {
                    for (const auto& profileName : profileNames) {
                        std::error_code error;
                        if (!std::filesystem::exists(ProfileStorage::Path(profileName), error) || error) {
                            continue;
                        }
                        const bool selected = profileName == window.sourceProfile;
                        if (GUI::Selectable(profileName.c_str(), selected)) {
                            window.sourceProfile = profileName;
                        }
                    }
                    GUI::EndCombo();
                }
            }

            GUI::Spacing();

            const std::string profileName{ window.name.data() };
            std::error_code error;
            const bool duplicateName = std::filesystem::exists(ProfileStorage::Path(profileName), error);
            const bool operationRunning = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
            const bool canCreate = !operationRunning && Profiles::IsValidName(profileName) && !duplicateName && !error && (!window.duplicate || sourceAvailable);
            std::string validationError;
            if (!profileName.empty() && !Profiles::IsValidName(profileName)) {
                validationError = Trans::Tr("Enter a valid profile name.");
            }
            else if (duplicateName) {
                validationError = Trans::Tr("A profile with this name already exists.");
            }
            if (CTAButton(Trans::Tr("Create").c_str(), canCreate, Color::kBackupButtonColors)) {
                const auto source = window.duplicate ? window.sourceProfile : std::string{};
                if (Profiles::Create(profileName, source, window.error)) {
                    RefreshProfileNames();
                    loaded = false;
                    window.open = false;
                }
            }
            GUI::SameLine(0.0F, 14.0F);
            if (GUI::Button(Trans::Tr("Cancel").c_str())) {
                window.open = false;
            }

            const auto& displayError = validationError.empty() ? window.error : validationError;
            if (!displayError.empty()) {
                GUI::TextWrapped("%s", displayError.c_str());
            }
        }
        GUI::End();
    }

    void ProfileMenu::RenderDeleteProfileWindow()
    {
        auto& window = deleteProfileWindow;
        if (!window.open) {
            return;
        }

        GUI::SetNextWindowSize(GUI::ImVec2{ 420.0F, 145.0F }, GUI::ImGuiCond_FirstUseEver);
        CenterNextWindow();
        const auto title = std::format("{}###Delete MCM Memory Profile", Trans::Tr("Delete Profile"));
        if (GUI::Begin(title.c_str(), std::addressof(window.open), GUI::ImGuiWindowFlags_NoCollapse)) {
            const auto message = std::format("{}: {}", Trans::Tr("Delete profile"), window.profile);
            GUI::TextWrapped("%s", message.c_str());
            GUI::Spacing();

            const bool operationRunning = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
            if (CTAButton(Trans::Tr("Delete").c_str(), !operationRunning, Color::kCancelButtonColors)) {
                if (Profiles::Delete(window.profile, window.error)) {
                    RefreshProfileNames();
                    loaded = false;
                    window.open = false;
                }
            }
            GUI::SameLine(0.0F, 14.0F);
            if (GUI::Button(Trans::Tr("Cancel").c_str())) {
                window.open = false;
            }

            if (!window.error.empty()) {
                GUI::TextWrapped("%s", window.error.c_str());
            }
        }
        GUI::End();
    }

    void ProfileMenu::RenderAutomation()
    {
        GUI::SeparatorText(Trans::Tr("Automation").c_str());

        auto& settings = GetSettings();
        bool changed{};
        if (GUI::Checkbox(Trans::Tr("Automatic backup").c_str(), std::addressof(settings.autoBackup))) {
            changed = true;
        }

        GUI::SameLine(0.0F, 20.0F);

        if (GUI::Checkbox(Trans::Tr("Automatic restore").c_str(), std::addressof(settings.autoRestore))) {
            changed = true;
        }
        if (changed && !SettingsStorage::Save()) {
            logger::error("MCM Memory menu could not save its automation settings");
        }
    }

    void ProfileMenu::RenderMCMTable(bool a_operationAvailable)
    {
        size_t visibleMCMCount{};
        for (const auto& mcm : mcms) {
            if (IsVisible(mcm)) {
                ++visibleMCMCount;
            }
        }
        if (visibleMCMCount == 0) {
            GUI::TextDisabled("%s", Trans::Tr(mcms.empty() ? "Waiting for MCM registration..." : "No MCMs match the current filter.").c_str());
            return;
        }

        constexpr size_t maximumVisibleRows{ 10 };
        const float tableHeight = GUI::GetFrameHeightWithSpacing() * static_cast<float>(std::min(visibleMCMCount, maximumVisibleRows) + 1);
        const auto tableFlags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_ScrollY;
        if (!GUI::BeginTable("Profile MCMs", 4, tableFlags, GUI::ImVec2{ 0.0F, tableHeight })) {
            return;
        }

        GUI::TableSetupColumn(Trans::Tr("Select").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 60.0F);
        GUI::TableSetupColumn(Trans::Tr("MCM").c_str(), GUI::ImGuiTableColumnFlags_WidthStretch);
        GUI::TableSetupColumn(Trans::Tr("Saved Settings").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 110.0F);
        GUI::TableSetupColumn(Trans::Tr("Auto Restore").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 105.0F);
        GUI::TableHeadersRow();

        auto& settings = GetSettings();
        bool settingsChanged{};
        for (auto& mcm : mcms) {
            if (!IsVisible(mcm)) {
                continue;
            }

            GUI::PushID(mcm.identity.modID.c_str());
            GUI::TableNextRow();
            GUI::TableSetColumnIndex(0);
            GUI::BeginDisabled(!mcm.available || !a_operationAvailable);
            GUI::Checkbox("##Selected", std::addressof(mcm.selected));
            GUI::EndDisabled();

            GUI::TableSetColumnIndex(1);
            const auto modName = GetDisplayModName(mcm.identity.modName);
            if (mcm.available) {
                GUI::TextUnformatted(modName.c_str());
            }
            else {
                GUI::TextDisabled("%s", modName.c_str());
            }
            if (mcm.available && mcm.settingCount > 0) {
                WrappedTooltip(Trans::Tr("Select this MCM to back up or restore its settings.").c_str());
            }
            else if (mcm.available) {
                WrappedTooltip(Trans::Tr("Select this MCM to back it up.").c_str());
            }
            else {
                WrappedTooltip(Trans::Tr("This MCM is saved in the profile but is not currently registered.").c_str());
            }

            GUI::TableSetColumnIndex(2);
            if (mcm.settingCount > 0) {
                GUI::Text("%u", mcm.settingCount);
            }
            else {
                GUI::TextDisabled("%s", Trans::Tr("None").c_str());
            }

            GUI::TableSetColumnIndex(3);
            bool autoRestore = settings.IsAutoRestoreEnabled(mcm.identity.modID);
            if (GUI::Checkbox("##AutoRestore", std::addressof(autoRestore))) {
                settings.SetAutoRestoreEnabled(mcm.identity.modID, autoRestore);
                settingsChanged = true;
            }
            GUI::PopID();
        }
        GUI::EndTable();

        if (settingsChanged && !SettingsStorage::Save()) {
            logger::error("MCM Memory menu could not save per-MCM automatic restore settings");
        }
    }

    void ProfileMenu::RenderMCMCounts(size_t a_registeredMCMCount, size_t a_selectedMCMCount) const
    {
        GUI::TextColored(Color::kCountText, "(");
        GUI::SameLine(0.0F, 0.0F);
        GUI::TextColored(Color::kCountNumber, "%zu", a_registeredMCMCount);
        GUI::SameLine(0.0F, 0.0F);
        GUI::TextColored(Color::kCountText, " %s, ", Trans::Tr("registered").c_str());
        GUI::SameLine(0.0F, 0.0F);
        GUI::TextColored(Color::kCountNumber, "%zu", a_selectedMCMCount);
        GUI::SameLine(0.0F, 0.0F);
        GUI::TextColored(Color::kCountText, " %s)", Trans::Tr("selected").c_str());
    }

    void ProfileMenu::RenderMCMs()
    {
        if (!GUI::CollapsingHeader(Trans::Tr("MCMs").c_str(), GUI::ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }
        if (NeedsRefresh()) {
            Refresh();
        }

        size_t registeredMCMCount{};
        for (const auto& mcm : mcms) {
            registeredMCMCount += mcm.available ? 1 : 0;
        }

        const auto backupStatus = Backup::GetSingleton()->GetStatus();
        const auto restoreStatus = Restore::GetSingleton()->GetStatus();
        const bool operationAvailable = IsGameLoaded() && backupStatus == OperationStatus::Idle && restoreStatus == OperationStatus::Idle;
        const auto backupLabel = Trans::Tr("Back Up Selected");
        const auto restoreLabel = Trans::Tr("Restore Selected");

        GUI::SetNextItemWidth(std::min(300.0F, GUI::GetContentRegionAvail().x));
        GUI::InputTextWithHint("##MCM Search", Trans::Tr("Search MCMs...").c_str(), search.data(), search.size());

        if (GUI::Button(Trans::Tr("Select All").c_str())) {
            SelectVisibleMCMs(true);
        }
        GUI::SameLine();
        if (GUI::Button(Trans::Tr("Clear").c_str())) {
            SelectVisibleMCMs(false);
        }
        GUI::SameLine();
        GUI::Checkbox(Trans::Tr("Hide unavailable MCMs").c_str(), std::addressof(hideUnavailable));

        const auto selectedMCMs = ReadSelectedMCMs();
        const bool selectedBackupAvailable = operationAvailable && !selectedMCMs.backup.empty();
        const bool selectedRestoreAvailable = operationAvailable && !selectedMCMs.restore.empty();
        GUI::SameLine();
        RenderMCMCounts(registeredMCMCount, selectedMCMs.backup.size());
        GUI::SameLine(0.0F, 28.0F);
        if (IconCTAButton(backupLabel.c_str(), selectedBackupAvailable, Icons::kSave, Color::kBackupButtonColors)) {
            Backup::GetSingleton()->StartSelected(selectedMCMs.backup);
        }

        GUI::SameLine(0.0F, 14.0F);
        if (IconCTAButton(restoreLabel.c_str(), selectedRestoreAvailable, Icons::kRestore, Color::kRestoreButtonColors)) {
            Restore::GetSingleton()->StartSelected(selectedMCMs.restore);
        }

        RenderMCMTable(operationAvailable);
    }

    void ProfileMenu::Render()
    {
        RenderProfileControls();
        RenderCreateProfileWindow();
        RenderDeleteProfileWindow();
        GUI::Spacing();
        RenderAutomation();
        GUI::Spacing();
        RenderMCMs();
    }
}
