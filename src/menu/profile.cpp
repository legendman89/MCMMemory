#include "menu/menu.hpp"
#include "menu/icons.hpp"
#include "menu/profile.hpp"
#include "menu/translate.hpp"
#include "menu/automation.hpp"
#include "profile/backup.hpp"
#include "profile/capture.hpp"
#include "profile/profile.hpp"
#include "profile/profiles.hpp"
#include "profile/restore.hpp"
#include "utils/helper.hpp"
#include "utils/time.hpp"

#include "settings.hpp"

namespace MCMMemory::Menu
{
    bool ProfileMCMRowOrder::operator()(const ProfileMCMRow& a_left, const ProfileMCMRow& a_right) const
    {
        if (originalOrder) {
            return a_left.originalIndex < a_right.originalIndex;
        }

        if (bySelected && a_left.selected != a_right.selected) {
            return descending ? a_left.selected : a_right.selected;
        }

        if (bySettingCount && a_left.settingCount != a_right.settingCount) {
            return descending ? a_left.settingCount > a_right.settingCount : a_left.settingCount < a_right.settingCount;
        }

        const auto leftName = GetDisplayModName(a_left.identity.modName);
        const auto rightName = GetDisplayModName(a_right.identity.modName);
        for (size_t index = 0; index < std::min(leftName.size(), rightName.size()); ++index) {
            const auto leftCharacter = ToLowerASCII(static_cast<uchar_t>(leftName[index]));
            const auto rightCharacter = ToLowerASCII(static_cast<uchar_t>(rightName[index]));
            if (leftCharacter != rightCharacter) {
                return descending ? leftCharacter > rightCharacter : leftCharacter < rightCharacter;
            }
        }

        if (leftName.size() != rightName.size()) {
            return descending ? leftName.size() > rightName.size() : leftName.size() < rightName.size();
        }

        return descending ? a_left.identity.modID > a_right.identity.modID : a_left.identity.modID < a_right.identity.modID;
    }

    void ProfileMenu::RenderProfileSelector()
    {
        if (profileNames.empty()) {
            RefreshProfileNames();
        }

        auto& settings = GetSettings();
        const bool operationRunning = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
        const bool disabled = operationRunning || IsEditingProfile();

        GUI::BeginDisabled(disabled);

        GUI::AlignTextToFramePadding();

        GUI::BoldTextColored({ Color::kCountNumber.x, Color::kCountNumber.y, Color::kCountNumber.z, Color::kCountNumber.w }, Trans::Tr("Profile.Active").c_str());

        GUI::SameLine(0.0F, 10.0F);

        std::vector<const char*> items;
        items.reserve(profileNames.size());
        int selected = -1;
        for (size_t index = 0; index < profileNames.size(); ++index) {
            items.push_back(profileNames[index].c_str());
            if (profileNames[index] == settings.activeProfile) {
                selected = static_cast<int>(index);
            }
        }

        GUI::BeginDisabled(items.empty());
        GUI::SetNextItemWidth(ProfileFieldWidth);
        const bool changed = GUI::Combo("##Active Profile", std::addressof(selected), items.data(), static_cast<int>(items.size()));
        GUI::EndDisabled();
        GUI::WrappedTooltip(Trans::Tr("Profile.Active.Tooltip").c_str());

        if (changed && !disabled && selected >= 0 && static_cast<size_t>(selected) < profileNames.size()) {
            std::string error;
            if (Profiles::Select(profileNames[selected], error)) {
                loaded = false;
            }
            else {
                logger::error("Profile selection failed: {}", Trans::Tr(error));
            }
        }

        GUI::SameLine(0.0F, 10.0F);

        if (IconCTAButton(Trans::Tr("Common.Action.Create").c_str(), true, Icons::kCreate, Color::kCreateButtonColors)) {
            createProfileWindow = {};
            createProfileWindow.open = true;
            createProfileWindow.sourceProfile = settings.activeProfile;
        }
        GUI::WrappedTooltip(Trans::Tr("Profile.Action.Create.Tooltip").c_str());

        std::error_code error;
        const bool selectedProfileExists = std::filesystem::exists(ProfileStorage::Path(), error) && !error;

        GUI::SameLine();

        if (IconCTAButton(Trans::Tr("Common.Action.Delete").c_str(), selectedProfileExists && profileNames.size() > 1, Icons::kDelete, Color::kCancelButtonColors)) {
            deleteProfileWindow = {};
            deleteProfileWindow.open = true;
            deleteProfileWindow.profile = settings.activeProfile;
        }
        GUI::WrappedTooltip(Trans::Tr("Profile.Action.Delete.Tooltip").c_str());

        GUI::EndDisabled();
    }

    void ProfileMenu::RenderProfileControls()
    {
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

        RenderProfileSelector();

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
            if (!mcm.selected) {
                continue;
            }
            selectedMCMs.selected.push_back(mcm.identity.modID);
            if (mcm.CanForget()) {
                selectedMCMs.forget.push_back(mcm.identity.modID);
            }
            if (!mcm.CanSelect()) {
                continue;
            }
            selectedMCMs.backup.push_back(mcm.identity.modID);
            if (mcm.settingCount > 0) {
                selectedMCMs.restore.push_back(mcm.identity.modID);
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
        row.originalIndex = mcms.size();
        row.selected = ContainsMCMID(a_selectedMCMs, row.identity.modID);
        mcms.push_back(std::move(row));
        return mcms.back();
    }

    void ProfileMenu::Refresh()
    {
        const auto selectedMCMs = ReadSelectedMCMs().selected;
        const bool currentGameLoaded = IsGameLoaded();
        if (currentGameLoaded != gameLoaded) {
            registryWait.Reset();
            registrySettled = false;
        }

        mcms.clear();

        sortPending = true;

        std::error_code error;
        profileAvailable = std::filesystem::exists(ProfileStorage::Path(), error) && !error;
        Profile profile;
        if (profileAvailable && ProfileStorage::Load(profile)) {
            for (const auto& setting : profile.settings) {
                ++FindOrAddMCM(setting.selection.identity, selectedMCMs).settingCount;
            }
        }

        if (currentGameLoaded) {
            const auto registeredMCMs = MCMRegistry().ReadRegisteredMCMs();
            const auto registryResult = registryWait.Update(registeredMCMs);
            registrySettled = registryResult == RegistryWaitResult::Ready && !MCMRegistry::IsRefreshing() && MCMKickerSupport::GetSingleton()->GetStatus() != MCMKickerSupport::Status::Waiting;
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
            if (!mcm.CanSelect() && !mcm.CanForget()) {
                mcm.selected = false;
            }
        }

        if (profileAvailable) {
            profileWriteTime = std::filesystem::last_write_time(ProfileStorage::Path(), error);
        }

        nextRegistryRefresh = TimeAfter(std::chrono::steady_clock::now(), registryRefreshInterval);
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
        sortPending = true;
        for (auto& mcm : mcms) {
            if (!a_selected) {
                mcm.selected = false;
                continue;
            }
            if ((mcm.CanSelect() || mcm.CanForget()) && IsVisible(mcm)) {
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
        const bool journalMenuOpen = IsJournalMenuOpen();
        const bool journalBlocksOperation = !operationRunning && journalMenuOpen;
        const bool operationAvailable = IsGameLoaded() && !operationRunning && !journalMenuOpen && !IsEditingProfile();
        std::string backupLabel = Trans::Tr("Profile.Action.BackUpNow");
        std::string restoreLabel = Trans::Tr("Profile.Action.RestoreNow");
        uint32_t backupIcon = Icons::kSave;
        uint32_t restoreIcon = Icons::kRestore;
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
        const auto backupTooltip = backupStatus == OperationStatus::Idle && journalBlocksOperation ? "Profile.Action.JournalMenuOpen.Tooltip" : backupStatus == OperationStatus::Idle ? "Profile.Action.BackUpNow.Tooltip" : "Profile.Action.CancelBackup.Tooltip";
        GUI::WrappedTooltip(Trans::Tr(backupTooltip).c_str());

        GUI::SameLine(0.0F, 14.0F);
        if (IconCTAButton(restoreLabel.c_str(), restoreEnabled, restoreIcon, *restoreColors, GUI::ImVec2{ a_restoreWidth, 0.0F })) {
            if (restoreStatus == OperationStatus::Running) {
                restore->Cancel();
            }
            else {
                restore->Start();
            }
        }
        const auto restoreTooltip = restoreStatus == OperationStatus::Idle && journalBlocksOperation ? "Profile.Action.JournalMenuOpen.Tooltip" : restoreStatus == OperationStatus::Idle ? "Profile.Action.RestoreNow.Tooltip" : "Profile.Action.CancelRestore.Tooltip";
        GUI::WrappedTooltip(Trans::Tr(restoreTooltip).c_str());
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

        const auto title = std::format("{}###Create MCM Memory Profile", Trans::Tr("Profile.Create.Title"));
        if (GUI::BeginWindow(title.c_str(), std::addressof(window.open), 540.0F, 320.0F)) {

            GUI::SetNextItemWidth(ProfileFieldWidth);
            if (GUI::InputText(Trans::Tr("Profile.Create.Name").c_str(), window.name.data(), window.name.size())) {
                window.error.clear();
            }

            GUI::Spacing();

            if (GUI::RadioButton(Trans::Tr("Profile.Create.Empty").c_str(), !window.duplicate)) {
                window.duplicate = false;
            }
            GUI::WrappedTooltip(Trans::Tr("Profile.Create.Empty.Tooltip").c_str());

            GUI::SameLine(0.0F, 20.0F);

            GUI::BeginDisabled(!sourceAvailable);

            if (GUI::RadioButton(Trans::Tr("Profile.Create.Duplicate").c_str(), window.duplicate)) {
                window.duplicate = true;
            }
            GUI::WrappedTooltip(Trans::Tr("Profile.Create.Duplicate.Tooltip").c_str());

            GUI::EndDisabled();

            if (window.duplicate) {

                GUI::Spacing();

                std::vector<const char*> sources;
                int selected = -1;
                for (const auto& profileName : profileNames) {
                    std::error_code error;
                    if (!std::filesystem::exists(ProfileStorage::Path(profileName), error) || error) {
                        continue;
                    }
                    if (profileName == window.sourceProfile) {
                        selected = static_cast<int>(sources.size());
                    }
                    sources.push_back(profileName.c_str());
                }
                GUI::SetNextItemWidth(ProfileFieldWidth);
                if (GUI::Combo(Trans::Tr("Profile.Create.CopyFrom").c_str(), std::addressof(selected), sources.data(), static_cast<int>(sources.size())) && selected >= 0 && static_cast<size_t>(selected) < sources.size()) {
                    window.sourceProfile = sources[selected];
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
        GUI::EndWindow();
    }

    bool ProfileMenu::RenderConfirmWindow(ConfirmWindow& a_window, std::string_view a_id, std::string_view a_titleKey, const std::string& a_message)
    {
        if (!a_window.open) {
            return false;
        }

        bool confirmed{};
        const auto title = std::format("{}###{}", Trans::Tr(a_titleKey), a_id);
        const float messageWidth = GUI::MeasureText(a_message.c_str()).width;
        const float windowWidth = std::max(520.0F, messageWidth + 80.0F);
        if (GUI::BeginWindow(title.c_str(), std::addressof(a_window.open), windowWidth, 170.0F, true)) {
            GUI::CenterNextItem(messageWidth);
            GUI::TextUnformatted(a_message.c_str());
            GUI::Spacing();

            const bool operationRunning = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
            const auto yesLabel = Trans::Tr("Common.Action.Yes");
            const auto cancelLabel = Trans::Tr("Common.Action.Cancel");
            const auto yesSize = MeasureCTAButton(yesLabel.c_str());
            const auto cancelSize = MeasureCTAButton(cancelLabel.c_str());
            const float buttonWidth = std::max(yesSize.x, cancelSize.x);
            const float buttonHeight = std::max(yesSize.y, cancelSize.y);
            const auto actualYesSize = GUI::MeasureButton(yesLabel.c_str(), buttonWidth, buttonHeight);
            const auto actualCancelSize = GUI::MeasureButton(cancelLabel.c_str(), buttonWidth, buttonHeight);
            constexpr float buttonSpacing{ 14.0F };
            GUI::CenterNextItem(actualYesSize.width + actualCancelSize.width + buttonSpacing);
            confirmed = CTAButton(yesLabel.c_str(), !operationRunning, Color::kCancelButtonColors, GUI::ImVec2{ buttonWidth, buttonHeight });

            GUI::SameLine(0.0F, buttonSpacing);

            if (CTAButton(cancelLabel.c_str(), true, Color::kNeutralButtonColors, GUI::ImVec2{ buttonWidth, buttonHeight })) {
                a_window.open = false;
            }

            // The caller sets this after the prompt is drawn, so a failure shows on the next frame.
            if (!a_window.error.empty()) {
                GUI::Spacing();
                GUI::TextWrapped("%s", Trans::Tr(a_window.error).c_str());
            }

            GUI::Spacing();
        }
        GUI::EndWindow();
        return confirmed;
    }

    void ProfileMenu::RenderForgetMCMsWindow()
    {
        auto& window = forgetMCMsWindow;
        if (!window.open) {
            return;
        }

        if (window.profile != GetSettings().activeProfile) {
            window.open = false;
            return;
        }

        const auto message = Trans::Format("Profile.Forget.Prompt", window.modIDs.size(), window.profile);
        if (!RenderConfirmWindow(window, "Forget MCM Memory Settings", "Profile.Forget.Title", message)) {
            return;
        }

        size_t settingCount{};
        if (ProfileStorage::ForgetMCMs(window.profile, window.modIDs, settingCount)) {
            Capture::GetSingleton()->ForgetMCMs(window.modIDs);
            logger::info("Removed {} settings from {} MCMs in profile '{}'", settingCount, window.modIDs.size(), window.profile);
            loaded = false;
            window.open = false;
        }
        else {
            window.error = "Profile.Forget.Failed";
        }
    }

    void ProfileMenu::RenderDeleteProfileWindow()
    {
        auto& window = deleteProfileWindow;
        if (!window.open) {
            return;
        }

        const auto message = Trans::Format("Profile.Delete.Prompt", window.profile);
        if (!RenderConfirmWindow(window, "Delete MCM Memory Profile", "Profile.Delete.Title", message)) {
            return;
        }

        if (Profiles::Delete(window.profile, window.error)) {
            RefreshProfileNames();
            loaded = false;
            window.open = false;
        }
    }

    void ProfileMenu::RenderMCMTable(bool a_selectionAvailable)
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
        const auto tableFlags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_BordersOuterH | GUI::ImGuiTableFlags_ScrollY | GUI::ImGuiTableFlags_Sortable | GUI::ImGuiTableFlags_SortTristate;
        if (!GUI::BeginTable("Profile MCMs", 4, tableFlags, GUI::ImVec2{ 0.0F, tableHeight })) {
            return;
        }

        const auto selectedLabel = Trans::Tr("Profile.MCM.Column.Selected");
        const auto savedSettingsLabel = Trans::Tr("Profile.MCM.Column.SavedSettings");
        const auto autoRestoreLabel = Trans::Tr("Profile.MCM.Column.AutoRestore");
        const float selectedWidth = std::max(GUI::GetFrameHeight(), GUI::MeasureTableHeader(selectedLabel.c_str()));
        GUI::TableSetupColumn(selectedLabel.c_str(), GUI::ImGuiTableColumnFlags_WidthFixed | GUI::ImGuiTableColumnFlags_PreferSortDescending, selectedWidth);
        GUI::TableSetupColumn(Trans::Tr("Common.MCM").c_str(), GUI::ImGuiTableColumnFlags_WidthStretch);
        GUI::TableSetupColumn(savedSettingsLabel.c_str(), GUI::ImGuiTableColumnFlags_WidthFixed | GUI::ImGuiTableColumnFlags_PreferSortDescending, std::max(160.0F, GUI::MeasureTableHeader(savedSettingsLabel.c_str())));
        GUI::TableSetupColumn(autoRestoreLabel.c_str(), GUI::ImGuiTableColumnFlags_WidthFixed | GUI::ImGuiTableColumnFlags_NoSort, std::max(105.0F, GUI::MeasureTableHeader(autoRestoreLabel.c_str())));
        GUI::TableSetupScrollFreeze(0, 1);
        GUI::TableHeadersRow();

        GUI::TableSort sort;
        if (GUI::ReadTableSort(sort, sortPending)) {
            ProfileMCMRowOrder order;
            order.originalOrder = sort.column < 0;
            order.bySelected = sort.column == 0;
            order.bySettingCount = sort.column == 2;
            order.descending = sort.descending;
            std::sort(mcms.begin(), mcms.end(), order);
            sortPending = false;
        }

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
            GUI::BeginDisabled((!mcm.CanSelect() && !mcm.CanForget()) || !a_selectionAvailable);

            GUI::CenterNextItem(GUI::GetFrameHeight());
            if (GUI::Checkbox("##Selected", std::addressof(mcm.selected))) {
                sortPending = true;
            }

            GUI::EndDisabled();
            GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Column.Selected.Tooltip").c_str());

            GUI::TableSetColumnIndex(1);
            GUI::AlignTextToFramePadding();
            const auto modName = GetDisplayModName(mcm.identity.modName);
            if (excluded) {
                GUI::TextDisabled("%s (%s)", modName.c_str(), Trans::Tr("Profile.MCM.Excluded").c_str());
            }
            else if (unresponsive) {
                GUI::TextDisabled("%s (%s)", modName.c_str(), Trans::Tr("Profile.MCM.Unresponsive").c_str());
            }
            else if (mcm.selected) {
                ColoredText(Color::kCountNumber, modName.c_str());
            }
            else if (mcm.available) {
                GUI::TextUnformatted(modName.c_str());
            }
            else {
                GUI::TextDisabled("%s", modName.c_str());
            }

            if (!IsEditingProfile() && Backup::GetSingleton()->GetStatus() == OperationStatus::Idle && Restore::GetSingleton()->GetStatus() == OperationStatus::Idle && GUI::IsItemClicked()) {
                pagesWindow.Open(mcm.identity);
            }
            
            if (excluded) {
                GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.Excluded").c_str());
            }
            else if (unresponsive) {
                GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.Unresponsive").c_str());
            }
            else if (mcm.available && mcm.settingCount > 0) {
                GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.BackupRestore").c_str());
            }
            else if (mcm.available) {
                GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.BackupOnly").c_str());
            }
            else {
                GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Tooltip.Unavailable").c_str());
            }

            GUI::TableSetColumnIndex(2);
            GUI::AlignTextToFramePadding();
            if (mcm.settingCount > 0) {
                if (mcm.selected) {
                    ColoredText(Color::kCountNumber, std::to_string(mcm.settingCount).c_str());
                }
                else {
                    GUI::Text("%u", mcm.settingCount);
                }
            }
            else {
                if (mcm.selected) {
                    ColoredText(Color::kCountNumber, Trans::Tr("Common.None").c_str());
                }
                else {
                    GUI::TextDisabled("%s", Trans::Tr("Common.None").c_str());
                }
            }

            GUI::TableSetColumnIndex(3);
            bool autoRestore = !excluded && !unresponsive && settings.IsAutoRestoreEnabled(mcm.identity.modID);
            GUI::BeginDisabled(excluded || unresponsive);
            GUI::CenterNextItem(GUI::GetFrameHeight());
            if (GUI::Checkbox("##AutoRestore", std::addressof(autoRestore))) {
                settings.SetAutoRestoreEnabled(mcm.identity.modID, autoRestore);
                settingsChanged = true;
            }
            GUI::EndDisabled();
            GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Column.AutoRestore.Tooltip").c_str());
            GUI::PopID();
        }
        GUI::EndTable();

        if (settingsChanged && !SettingsStorage::Save()) {
            logger::error("MCM Memory menu could not save per-MCM automatic restore settings");
        }
    }

    void ProfileMenu::RenderMCMCounts(size_t a_registeredMCMCount, size_t a_selectedMCMCount) const
    {
        GUI::AlignTextToFramePadding();
        ColoredText(Color::kCountText, "(");
        GUI::SameLine(0.0F, 0.0F);
        ColoredText(Color::kCountNumber, std::to_string(a_registeredMCMCount).c_str());
        GUI::SameLine(0.0F, 0.0F);
        ColoredText(Color::kCountText, std::format(" {}, ", Trans::Tr("Profile.MCM.Count.Registered")).c_str());
        GUI::SameLine(0.0F, 0.0F);
        ColoredText(Color::kCountNumber, std::to_string(a_selectedMCMCount).c_str());
        GUI::SameLine(0.0F, 0.0F);
        ColoredText(Color::kCountText, std::format(" {})", Trans::Tr("Profile.MCM.Count.Selected")).c_str());
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
        const bool journalMenuOpen = IsJournalMenuOpen();
        const bool operationAvailable = IsGameLoaded() && backupStatus == OperationStatus::Idle && restoreStatus == OperationStatus::Idle && !IsEditingProfile();
        const bool journalBlocksOperation = operationAvailable && journalMenuOpen;
        const auto backupLabel = Trans::Tr("Profile.MCM.BackUpSelected");
        const auto restoreLabel = Trans::Tr("Profile.MCM.RestoreSelected");
        const auto forgetLabel = Trans::Tr("Profile.MCM.ForgetSelected");
        const bool operationRunning = backupStatus != OperationStatus::Idle || restoreStatus != OperationStatus::Idle;

        GUI::Spacing();
        GUI::Spacing();

        GUI::SetNextItemWidth(std::min(500.0F, GUI::GetAvailableWidth()));
        GUI::InputTextWithHint("##MCM Search", Trans::Tr("Profile.MCM.SearchHint").c_str(), search.data(), search.size());

        GUI::Spacing();
        GUI::Spacing();

        const auto toolbarFlags = GUI::ImGuiTableFlags_SizingFixedFit | GUI::ImGuiTableFlags_NoSavedSettings;
        if (GUI::BeginTable("##MCMControls", 3, toolbarFlags)) {
            GUI::TableSetupColumn("Selection", GUI::ImGuiTableColumnFlags_WidthFixed);
            GUI::TableSetupColumn("Spacing", GUI::ImGuiTableColumnFlags_WidthStretch);
            GUI::TableSetupColumn("Operations", GUI::ImGuiTableColumnFlags_WidthFixed);
            GUI::TableNextRow();
            GUI::TableSetColumnIndex(0);

            if (GUI::Button(Trans::Tr("Profile.MCM.SelectAll").c_str())) {
                SelectVisibleMCMs(true);
            }
            GUI::WrappedTooltip(Trans::Tr("Profile.MCM.SelectAll.Tooltip").c_str());

            GUI::SameLine(0.0F, 14.0F);

            if (GUI::Button(Trans::Tr("Common.Action.Clear").c_str())) {
                SelectVisibleMCMs(false);
            }
            GUI::WrappedTooltip(Trans::Tr("Profile.MCM.Clear.Tooltip").c_str());

            GUI::SameLine(0.0F, 14.0F);
            GUI::Checkbox(Trans::Tr("Profile.MCM.HideUnavailable").c_str(), std::addressof(hideUnavailable));
            GUI::HelpMarker(Trans::Tr("Profile.MCM.HideUnavailable.Tooltip").c_str());

            const auto selectedMCMs = ReadSelectedMCMs();
            const bool selectedBackupAvailable = operationAvailable && !journalMenuOpen && !selectedMCMs.backup.empty();
            const bool selectedRestoreAvailable = operationAvailable && !journalMenuOpen && !selectedMCMs.restore.empty();

            GUI::SameLine(0.0F, 18.0F);

            RenderMCMCounts(registeredMCMCount, selectedMCMs.selected.size());

            GUI::TableSetColumnIndex(2);

            if (IconCTAButton(backupLabel.c_str(), selectedBackupAvailable, Icons::kSave, Color::kBackupButtonColors)) {
                Backup::GetSingleton()->StartSelected(selectedMCMs.backup);
            }
            GUI::WrappedTooltip(Trans::Tr(journalBlocksOperation ? "Profile.Action.JournalMenuOpen.Tooltip" : "Profile.MCM.BackUpSelected.Tooltip").c_str());

            GUI::SameLine(0.0F, 14.0F);

            if (IconCTAButton(restoreLabel.c_str(), selectedRestoreAvailable, Icons::kRestore, Color::kRestoreButtonColors)) {
                Restore::GetSingleton()->StartSelected(selectedMCMs.restore);
            }
            GUI::WrappedTooltip(Trans::Tr(journalBlocksOperation ? "Profile.Action.JournalMenuOpen.Tooltip" : "Profile.MCM.RestoreSelected.Tooltip").c_str());

            GUI::SameLine(0.0F, 14.0F);

            // Only the profile file is touched, so this does not wait for the Journal Menu to close.
            if (IconCTAButton(forgetLabel.c_str(), !operationRunning && !IsEditingProfile() && !selectedMCMs.forget.empty(), Icons::kDelete, Color::kCancelButtonColors)) {
                forgetMCMsWindow = {};
                forgetMCMsWindow.open = true;
                forgetMCMsWindow.profile = GetSettings().activeProfile;
                forgetMCMsWindow.modIDs = selectedMCMs.forget;
            }
            GUI::WrappedTooltip(Trans::Tr("Profile.MCM.ForgetSelected.Tooltip").c_str());

            GUI::EndTable();
        }

        GUI::Spacing();

        RenderMCMTable(!operationRunning && !IsEditingProfile());
    }

    void ProfileMenu::Render()
    {
        GUI::Spacing();

        RenderProfileControls();

        RenderCreateProfileWindow();

        RenderDeleteProfileWindow();

        RenderForgetMCMsWindow();

        GUI::Spacing();
        GUI::Spacing();

        RenderAutomation();

        GUI::Spacing();
        GUI::Spacing();

        RenderMCMs();

        pagesWindow.Render();
    }
}
