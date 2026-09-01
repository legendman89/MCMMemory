#include "menu/profile.hpp"

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

        GUI::AlignTextToFramePadding();

        BoldTextColored(Color::kCountNumber, Trans::Tr("Profile.Active").c_str());

        GUI::SameLine(0.0F, 10.0F);

        GUI::SetNextItemWidth(ProfileFieldWidth);
        const bool profileComboOpen = BeginOpaqueCombo("##Active Profile", settings.activeProfile.c_str());
        WrappedTooltip(Trans::Tr("Profile.Active.Tooltip").c_str());
        if (profileComboOpen) {
            for (const auto& profileName : profileNames) {
                const bool selected = profileName == settings.activeProfile;
                if (GUI::Selectable(profileName.c_str(), selected)) {
                    std::string error;
                    if (Profiles::Select(profileName, error)) {
                        loaded = false;
                    }
                    else {
                        logger::error("Profile selection failed: {}", Trans::Tr(error));
                    }
                }
            }
            GUI::EndCombo();
        }

        GUI::SameLine(0.0F, 10.0F);

        if (IconCTAButton(Trans::Tr("Common.Action.Create").c_str(), true, Icons::kCreate, Color::kCreateButtonColors)) {
            createProfileWindow = {};
            createProfileWindow.open = true;
            createProfileWindow.sourceProfile = settings.activeProfile;
        }
        WrappedTooltip(Trans::Tr("Profile.Action.Create.Tooltip").c_str());

        std::error_code error;
        const bool selectedProfileExists = std::filesystem::exists(ProfileStorage::Path(), error) && !error;

        GUI::SameLine();

        if (IconCTAButton(Trans::Tr("Common.Action.Delete").c_str(), selectedProfileExists && profileNames.size() > 1, Icons::kDelete, Color::kCancelButtonColors)) {
            deleteProfileWindow = {};
            deleteProfileWindow.open = true;
            deleteProfileWindow.profile = settings.activeProfile;
        }
        WrappedTooltip(Trans::Tr("Profile.Action.Delete.Tooltip").c_str());

        GUI::EndDisabled();
    }

    void ProfileMenu::RenderProfileControls()
    {
        const auto backupStatus = Backup::GetSingleton()->GetStatus();
        const auto restoreStatus = Restore::GetSingleton()->GetStatus();
        const bool operationRunning = backupStatus != OperationStatus::Idle || restoreStatus != OperationStatus::Idle;
        const bool profileEditing = createProfileWindow.open || deleteProfileWindow.open;
        const auto backupLabel = Trans::Tr("Profile.Action.BackUpNow");
        const auto restoreLabel = Trans::Tr("Profile.Action.RestoreNow");
        const auto cancelLabel = Trans::Tr("Profile.Action.CancelNow");
        const auto stoppingLabel = Trans::Tr("Profile.Action.Stopping");
        const auto backupMetrics = MeasureIconButton(backupLabel.c_str(), Icons::kSave);
        const auto restoreMetrics = MeasureIconButton(restoreLabel.c_str(), Icons::kRestore);
        const auto cancelMetrics = MeasureIconButton(cancelLabel.c_str(), Icons::kCancel);
        const auto stoppingMetrics = MeasureIconButton(stoppingLabel.c_str(), Icons::kCancel);
        const float backupWidth = std::max({ backupMetrics.buttonSize.x, cancelMetrics.buttonSize.x, stoppingMetrics.buttonSize.x });
        const float restoreWidth = std::max({ restoreMetrics.buttonSize.x, cancelMetrics.buttonSize.x, stoppingMetrics.buttonSize.x });
        constexpr float operationSpacing{ 14.0F };
        const float operationWidth = backupWidth + operationSpacing + restoreWidth;
        const float operationHeight = std::max({ backupMetrics.buttonSize.y, restoreMetrics.buttonSize.y, cancelMetrics.buttonSize.y, stoppingMetrics.buttonSize.y });

        const auto* style = GUI::GetStyle();
        const float itemSpacing = style ? style->ItemSpacing.x : 8.0F;
        const auto profileLabel = Trans::Tr("Profile.Active");
        const auto createLabel = Trans::Tr("Common.Action.Create");
        const auto deleteLabel = Trans::Tr("Common.Action.Delete");
        const auto createMetrics = MeasureIconButton(createLabel.c_str(), Icons::kCreate);
        const auto deleteMetrics = MeasureIconButton(deleteLabel.c_str(), Icons::kDelete);
        const float profileWidth = GUI::CalcTextSize(profileLabel.c_str()).x + itemSpacing + ProfileFieldWidth + 10.0F + createMetrics.buttonSize.x + itemSpacing + deleteMetrics.buttonSize.x;
        const float profileHeight = std::max({ GUI::GetFrameHeight(), createMetrics.buttonSize.y, deleteMetrics.buttonSize.y });

        const GUI::ImVec2 start = GUI::GetCursorPos();
        const GUI::ImVec2 available = GUI::GetContentRegionAvail();
        const bool singleRow = profileWidth + itemSpacing + operationWidth <= available.x;

        RenderProfileSelector(operationRunning || profileEditing);

        const float operationX = singleRow ? start.x + std::max(0.0F, available.x - operationWidth) : start.x;
        const float operationY = singleRow ? start.y : start.y + profileHeight + itemSpacing;
        GUI::SetCursorPos(GUI::ImVec2{ operationX, operationY });
        RenderOperationButtons(backupWidth, restoreWidth);

        const float controlsHeight = singleRow ? std::max(profileHeight, operationHeight) : profileHeight + itemSpacing + operationHeight;
        GUI::SetCursorPos(GUI::ImVec2{ start.x, start.y + controlsHeight });
    }

    bool ProfileMenu::NeedsRefresh() const
    {
        if (!loaded) {
            return true;
        }
        if (IsGameLoaded() != gameLoaded) {
            return true;
        }
        if (MCMRegistry::CacheGeneration() != registryCacheGeneration) {
            return true;
        }
        if (MCMCallWatch::UnavailableGeneration() != unavailableGeneration) {
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
            if (mcm.selected && mcm.CanSelect()) {
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
            registrySettled = registryResult == RegistryWaitResult::Ready && !MCMRegistry::IsRefreshing();
            if (registryResult == RegistryWaitResult::Expired) {
                registryWait.Reset();
            }
            for (const auto& registeredMCM : registeredMCMs) {
                auto& mcm = FindOrAddMCM(registeredMCM.identity, selectedMCMs);
                mcm.identity = registeredMCM.identity;
                mcm.available = true;
            }
            if (!registrySettled) {
                MCMRegistry::Refresh();
            }
        }
        gameLoaded = currentGameLoaded;
        for (auto& mcm : mcms) {
            mcm.unresponsive = MCMCallWatch::IsUnavailable(mcm.identity.modID);
            if (!mcm.CanSelect()) {
                mcm.selected = false;
            }
        }

        if (profileAvailable) {
            profileWriteTime = std::filesystem::last_write_time(ProfileStorage::Path(), error);
        }
        nextRegistryRefresh = std::chrono::steady_clock::now() + registryRefreshInterval;
        registryCacheGeneration = MCMRegistry::CacheGeneration();
        unavailableGeneration = MCMCallWatch::UnavailableGeneration();
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
            if (mcm.CanSelect() && IsVisible(mcm)) {
                mcm.selected = true;
            }
        }
    }

    void ProfileMenu::RenderOperationButtons(float a_backupWidth, float a_restoreWidth)
    {
        auto* backup = Backup::GetSingleton();
        auto* restore = Restore::GetSingleton();
        const auto backupStatus = backup->GetStatus();
        const auto restoreStatus = restore->GetStatus();
        const bool operationRunning = backupStatus != OperationStatus::Idle || restoreStatus != OperationStatus::Idle;
        const bool operationAvailable = IsGameLoaded() && !operationRunning && !createProfileWindow.open && !deleteProfileWindow.open;
        std::string backupLabel = Trans::Tr("Profile.Action.BackUpNow");
        std::string restoreLabel = Trans::Tr("Profile.Action.RestoreNow");
        unsigned backupIcon = Icons::kSave;
        unsigned restoreIcon = Icons::kRestore;
        const Color::CTAColors* backupColors = std::addressof(Color::kBackupButtonColors);
        const Color::CTAColors* restoreColors = std::addressof(Color::kRestoreButtonColors);
        bool backupEnabled = operationAvailable;
        bool restoreEnabled = operationAvailable;

        if (backupStatus == OperationStatus::Running) {
            backupLabel = Trans::Tr("Profile.Action.CancelNow");
            backupIcon = Icons::kCancel;
            backupColors = std::addressof(Color::kCancelButtonColors);
            backupEnabled = true;
        }
        else if (backupStatus == OperationStatus::Stopping) {
            backupLabel = Trans::Tr("Profile.Action.Stopping");
            backupIcon = Icons::kCancel;
        }
        if (restoreStatus == OperationStatus::Running) {
            restoreLabel = Trans::Tr("Profile.Action.CancelNow");
            restoreIcon = Icons::kCancel;
            restoreColors = std::addressof(Color::kCancelButtonColors);
            restoreEnabled = true;
        }
        else if (restoreStatus == OperationStatus::Stopping) {
            restoreLabel = Trans::Tr("Profile.Action.Stopping");
            restoreIcon = Icons::kCancel;
        }

        if (IconCTAButton(backupLabel.c_str(), backupEnabled, backupIcon, *backupColors, GUI::ImVec2{ a_backupWidth, 0.0F })) {
            if (backupStatus == OperationStatus::Running) {
                backup->Cancel();
            }
            else {
                backup->Start();
            }
        }
        WrappedTooltip(Trans::Tr(backupStatus == OperationStatus::Idle ? "Profile.Action.BackUpNow.Tooltip" : "Profile.Action.CancelBackup.Tooltip").c_str());

        GUI::SameLine(0.0F, 14.0F);
        if (IconCTAButton(restoreLabel.c_str(), restoreEnabled, restoreIcon, *restoreColors, GUI::ImVec2{ a_restoreWidth, 0.0F })) {
            if (restoreStatus == OperationStatus::Running) {
                restore->Cancel();
            }
            else {
                restore->Start();
            }
        }
        WrappedTooltip(Trans::Tr(restoreStatus == OperationStatus::Idle ? "Profile.Action.RestoreNow.Tooltip" : "Profile.Action.CancelRestore.Tooltip").c_str());
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

        GUI::SetNextWindowSize(GUI::ImVec2{ 440.0F, 300.0F }, GUI::ImGuiCond_FirstUseEver);
        CenterNextWindow();
        const auto title = std::format("{}###Create MCM Memory Profile", Trans::Tr("Profile.Create.Title"));
        if (GUI::Begin(title.c_str(), std::addressof(window.open), GUI::ImGuiWindowFlags_NoCollapse)) {

            GUI::SetNextItemWidth(ProfileFieldWidth);
            if (GUI::InputText(Trans::Tr("Profile.Create.Name").c_str(), window.name.data(), window.name.size())) {
                window.error.clear();
            }

            GUI::Spacing();

            if (GUI::RadioButton(Trans::Tr("Profile.Create.Empty").c_str(), !window.duplicate)) {
                window.duplicate = false;
            }
            WrappedTooltip(Trans::Tr("Profile.Create.Empty.Tooltip").c_str());

            GUI::SameLine(0.0F, 20.0F);

            GUI::BeginDisabled(!sourceAvailable);

            if (GUI::RadioButton(Trans::Tr("Profile.Create.Duplicate").c_str(), window.duplicate)) {
                window.duplicate = true;
            }
            WrappedTooltip(Trans::Tr("Profile.Create.Duplicate.Tooltip").c_str());

            GUI::EndDisabled();

            if (window.duplicate) {

                GUI::Spacing();

                GUI::SetNextItemWidth(ProfileFieldWidth);
                if (BeginOpaqueCombo(Trans::Tr("Profile.Create.CopyFrom").c_str(), window.sourceProfile.c_str())) {
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
            const bool duplicateName = !profileName.empty() && std::filesystem::exists(ProfileStorage::Path(profileName), error);
            const bool operationRunning = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
            const bool canCreate = !operationRunning && Profiles::IsValidName(profileName) && !duplicateName && !error && (!window.duplicate || sourceAvailable);
            std::string validationError;
            if (!profileName.empty() && !Profiles::IsValidName(profileName)) {
                validationError = "Profile.Error.InvalidName";
            }
            else if (duplicateName) {
                validationError = "Profile.Error.DuplicateName";
            }
            if (CTAButton(Trans::Tr("Common.Action.Create").c_str(), canCreate, Color::kCreateButtonColors)) {
                const auto source = window.duplicate ? window.sourceProfile : std::string{};
                if (Profiles::Create(profileName, source, window.error)) {
                    RefreshProfileNames();
                    loaded = false;
                    window.open = false;
                }
            }

            GUI::SameLine(0.0F, 14.0F);

            if (CTAButton(Trans::Tr("Common.Action.Cancel").c_str(), true, Color::kNeutralButtonColors)) {
                window.open = false;
            }

            const auto& displayError = validationError.empty() ? window.error : validationError;
            if (!displayError.empty()) {
                GUI::Spacing();
                GUI::TextWrapped("%s", Trans::Tr(displayError).c_str());
            }

            GUI::Spacing();
        }
        GUI::End();
    }

    void ProfileMenu::RenderDeleteProfileWindow()
    {
        auto& window = deleteProfileWindow;
        if (!window.open) {
            return;
        }

        GUI::SetNextWindowSize(GUI::ImVec2{ 420.0F, 160.0F }, GUI::ImGuiCond_FirstUseEver);
        CenterNextWindow();
        const auto title = std::format("{}###Delete MCM Memory Profile", Trans::Tr("Profile.Delete.Title"));
        if (GUI::Begin(title.c_str(), std::addressof(window.open), GUI::ImGuiWindowFlags_NoCollapse)) {
            const auto message = Trans::Format("Profile.Delete.Prompt", window.profile);
            CenterNextItem(GUI::CalcTextSize(message.c_str()).x);
            GUI::TextUnformatted(message.c_str());
            GUI::Spacing();

            const bool operationRunning = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
            const auto yesLabel = Trans::Tr("Common.Action.Yes");
            const auto cancelLabel = Trans::Tr("Common.Action.Cancel");
            const auto yesSize = MeasureCTAButton(yesLabel.c_str());
            const auto cancelSize = MeasureCTAButton(cancelLabel.c_str());
            const float buttonWidth = std::max(yesSize.x, cancelSize.x);
            const float buttonHeight = std::max(yesSize.y, cancelSize.y);
            constexpr float buttonSpacing{ 14.0F };
            CenterNextItem(buttonWidth * 2.0F + buttonSpacing);
            if (CTAButton(yesLabel.c_str(), !operationRunning, Color::kCancelButtonColors, GUI::ImVec2{ buttonWidth, buttonHeight })) {
                if (Profiles::Delete(window.profile, window.error)) {
                    RefreshProfileNames();
                    loaded = false;
                    window.open = false;
                }
            }

            GUI::SameLine(0.0F, buttonSpacing);

            if (CTAButton(cancelLabel.c_str(), true, Color::kNeutralButtonColors, GUI::ImVec2{ buttonWidth, buttonHeight })) {
                window.open = false;
            }

            if (!window.error.empty()) {
                GUI::Spacing();
                GUI::TextWrapped("%s", Trans::Tr(window.error).c_str());
            }

            GUI::Spacing();
        }
        GUI::End();
    }

    void ProfileMenu::RenderAutomation()
    {
        auto& settings = GetSettings();
        bool changed{};
        if (GUI::Checkbox(Trans::Tr("Profile.Automation.Backup").c_str(), std::addressof(settings.autoBackup))) {
            changed = true;
        }
        HelpMarker(Trans::Tr("Profile.Automation.Backup.Tooltip").c_str());

        GUI::SameLine(0.0F, 20.0F);

        if (GUI::Checkbox(Trans::Tr("Profile.Automation.Restore").c_str(), std::addressof(settings.autoRestore))) {
            changed = true;
        }
        HelpMarker(Trans::Tr("Profile.Automation.Restore.Tooltip").c_str());
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
            GUI::TextDisabled("%s", Trans::Tr(mcms.empty() ? "Profile.MCM.Waiting" : "Profile.MCM.NoMatches").c_str());
            return;
        }

        constexpr size_t maximumVisibleRows{ 10 };
        const float tableHeight = GUI::GetFrameHeightWithSpacing() * static_cast<float>(std::min(visibleMCMCount, maximumVisibleRows) + 1);
        const auto tableFlags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_BordersOuterH | GUI::ImGuiTableFlags_ScrollY;
        if (!GUI::BeginTable("Profile MCMs", 4, tableFlags, GUI::ImVec2{ 0.0F, tableHeight })) {
            return;
        }

        GUI::TableSetupColumn(Trans::Tr("Profile.MCM.Column.Selected").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 75.0F);
        GUI::TableSetupColumn(Trans::Tr("Common.MCM").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 600.0F);
        GUI::TableSetupColumn(Trans::Tr("Profile.MCM.Column.SavedSettings").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 160.0F);
        GUI::TableSetupColumn(Trans::Tr("Profile.MCM.Column.AutoRestore").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 105.0F);
        GUI::TableNextRow(GUI::ImGuiTableRowFlags_Headers);
        GUI::TableSetColumnIndex(0);
        const auto selectedLabel = Trans::Tr("Profile.MCM.Column.Selected");
        CenterNextItem(GUI::CalcTextSize(selectedLabel.c_str()).x);
        GUI::TextUnformatted(selectedLabel.c_str());
        GUI::TableSetColumnIndex(1);
        GUI::TextUnformatted(Trans::Tr("Common.MCM").c_str());
        GUI::TableSetColumnIndex(2);
        GUI::TextUnformatted(Trans::Tr("Profile.MCM.Column.SavedSettings").c_str());
        GUI::TableSetColumnIndex(3);
        const auto autoRestoreLabel = Trans::Tr("Profile.MCM.Column.AutoRestore");
        CenterNextItem(GUI::CalcTextSize(autoRestoreLabel.c_str()).x);
        GUI::TextUnformatted(autoRestoreLabel.c_str());

        auto& settings = GetSettings();
        bool settingsChanged{};
        for (auto& mcm : mcms) {
            if (!IsVisible(mcm)) {
                continue;
            }

            GUI::PushID(mcm.identity.modID.c_str());
            const bool excluded = mcm.IsExcluded();
            const bool unresponsive = mcm.IsUnresponsive();

            GUI::TableNextRow();

            GUI::TableSetColumnIndex(0);
            GUI::BeginDisabled(!mcm.CanSelect() || !a_operationAvailable);

            CenterNextItem(GUI::GetFrameHeight());
            GUI::Checkbox("##Selected", std::addressof(mcm.selected));

            GUI::EndDisabled();
            WrappedTooltip(Trans::Tr("Profile.MCM.Column.Selected.Tooltip").c_str());

            GUI::TableSetColumnIndex(1);
            const auto modName = GetDisplayModName(mcm.identity.modName);
            if (excluded) {
                GUI::TextDisabled("%s (%s)", modName.c_str(), Trans::Tr("Profile.MCM.Excluded").c_str());
            }
            else if (unresponsive) {
                GUI::TextDisabled("%s (%s)", modName.c_str(), Trans::Tr("Profile.MCM.Unresponsive").c_str());
            }
            else if (mcm.selected) {
                GUI::TextColored(Color::kCountNumber, "%s", modName.c_str());
            }
            else if (mcm.available) {
                GUI::TextUnformatted(modName.c_str());
            }
            else {
                GUI::TextDisabled("%s", modName.c_str());
            }
            if (excluded) {
                WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.Excluded").c_str());
            }
            else if (unresponsive) {
                WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.Unresponsive").c_str());
            }
            else if (mcm.available && mcm.settingCount > 0) {
                WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.BackupRestore").c_str());
            }
            else if (mcm.available) {
                WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.BackupOnly").c_str());
            }
            else {
                WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.Unavailable").c_str());
            }

            GUI::TableSetColumnIndex(2);
            if (mcm.settingCount > 0) {
                if (mcm.selected) {
                    GUI::TextColored(Color::kCountNumber, "%u", mcm.settingCount);
                }
                else {
                    GUI::Text("%u", mcm.settingCount);
                }
            }
            else {
                if (mcm.selected) {
                    GUI::TextColored(Color::kCountNumber, "%s", Trans::Tr("Common.None").c_str());
                }
                else {
                    GUI::TextDisabled("%s", Trans::Tr("Common.None").c_str());
                }
            }

            GUI::TableSetColumnIndex(3);
            bool autoRestore = !excluded && !unresponsive && settings.IsAutoRestoreEnabled(mcm.identity.modID);
            GUI::BeginDisabled(excluded || unresponsive);
            CenterNextItem(GUI::GetFrameHeight());
            if (GUI::Checkbox("##AutoRestore", std::addressof(autoRestore))) {
                settings.SetAutoRestoreEnabled(mcm.identity.modID, autoRestore);
                settingsChanged = true;
            }
            GUI::EndDisabled();
            WrappedTooltip(Trans::Tr("Profile.MCM.Column.AutoRestore.Tooltip").c_str());
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
        GUI::TextColored(Color::kCountText, " %s, ", Trans::Tr("Profile.MCM.Count.Registered").c_str());
        GUI::SameLine(0.0F, 0.0F);
        GUI::TextColored(Color::kCountNumber, "%zu", a_selectedMCMCount);
        GUI::SameLine(0.0F, 0.0F);
        GUI::TextColored(Color::kCountText, " %s)", Trans::Tr("Profile.MCM.Count.Selected").c_str());
    }

    void ProfileMenu::RenderMCMs()
    {
        if (!GUI::CollapsingHeader(Trans::Tr("Profile.MCM.Header").c_str(), GUI::ImGuiTreeNodeFlags_DefaultOpen)) {
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
        const auto backupLabel = Trans::Tr("Profile.MCM.BackUpSelected");
        const auto restoreLabel = Trans::Tr("Profile.MCM.RestoreSelected");

        GUI::Spacing();

        GUI::SetNextItemWidth(std::min(500.0F, GUI::GetContentRegionAvail().x));
        GUI::InputTextWithHint("##MCM Search", Trans::Tr("Profile.MCM.SearchHint").c_str(), search.data(), search.size());

        GUI::Spacing();

        if (GUI::Button(Trans::Tr("Profile.MCM.SelectAll").c_str())) {
            SelectVisibleMCMs(true);
        }
        WrappedTooltip(Trans::Tr("Profile.MCM.SelectAll.Tooltip").c_str());

        GUI::SameLine(0.0F, 14.0F);

        if (GUI::Button(Trans::Tr("Common.Action.Clear").c_str())) {
            SelectVisibleMCMs(false);
        }
        WrappedTooltip(Trans::Tr("Profile.MCM.Clear.Tooltip").c_str());

        GUI::SameLine(0.0F, 14.0F);
        GUI::Checkbox(Trans::Tr("Profile.MCM.HideUnavailable").c_str(), std::addressof(hideUnavailable));
        HelpMarker(Trans::Tr("Profile.MCM.HideUnavailable.Tooltip").c_str());

        const auto selectedMCMs = ReadSelectedMCMs();
        const bool selectedBackupAvailable = operationAvailable && !selectedMCMs.backup.empty();
        const bool selectedRestoreAvailable = operationAvailable && !selectedMCMs.restore.empty();

        GUI::SameLine(0.0F, 18.0F);

        RenderMCMCounts(registeredMCMCount, selectedMCMs.backup.size());

        GUI::SameLine(0.0F, 28.0F);

        if (IconCTAButton(backupLabel.c_str(), selectedBackupAvailable, Icons::kSave, Color::kBackupButtonColors)) {
            Backup::GetSingleton()->StartSelected(selectedMCMs.backup);
        }
        WrappedTooltip(Trans::Tr("Profile.MCM.BackUpSelected.Tooltip").c_str());

        GUI::SameLine(0.0F, 14.0F);

        if (IconCTAButton(restoreLabel.c_str(), selectedRestoreAvailable, Icons::kRestore, Color::kRestoreButtonColors)) {
            Restore::GetSingleton()->StartSelected(selectedMCMs.restore);
        }
        WrappedTooltip(Trans::Tr("Profile.MCM.RestoreSelected.Tooltip").c_str());

        GUI::Spacing();

        RenderMCMTable(operationAvailable);
    }

    void ProfileMenu::Render()
    {
        GUI::Spacing();

        RenderProfileControls();

        RenderCreateProfileWindow();

        RenderDeleteProfileWindow();

        GUI::Spacing();
        GUI::Spacing();

        RenderAutomation();

        GUI::Spacing();
        GUI::Spacing();

        RenderMCMs();
    }
}
