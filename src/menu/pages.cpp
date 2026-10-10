#include "menu/menu.hpp"
#include "menu/pages.hpp"
#include "menu/translate.hpp"
#include "mcm/mcm_script.hpp"
#include "mcm/mcm_registry.hpp"
#include "profile/backup.hpp"
#include "profile/restore.hpp"
#include "profile/profile.hpp"

#include "settings.hpp"

namespace MCMMemory::Menu
{
    void MCMPagesWindow::Open(const MCMIdentity& a_identity)
    {
        identity = a_identity;
        profile = GetSettings().activeProfile;
        pages.clear();
        error.clear();
        open = true;
        Refresh(true);
    }

    void MCMPagesWindow::AddPage(std::string_view a_name, int a_index, bool a_available, bool a_uniqueName)
    {
        // Skip blank pages but keep unnamed pages (indexed as -1).
        if (a_index >= 0 && GetDisplayText(a_name).find_first_not_of(" \t\r\n\f\v") == std::string::npos) {
            return;
        }

        size_t nameMatches{};
        MCMPageRow* namedPage{};
        for (auto& page : pages) {
            if (page.name == a_name) {
                ++nameMatches;
                namedPage = std::addressof(page);
            }
        }

        // Unique MCM names are stable in reordering.
        // unnamed pages can use their index.
        if (!a_name.empty() && nameMatches == 1 && !namedPage->matchIndex && (a_uniqueName || (!a_available && namedPage->available))) {
            if (a_available) {
                namedPage->index = a_index;
                namedPage->available = true;
            }
            return;
        }

        for (auto& page : pages) {
            if (page.name == a_name && page.index == a_index) {
                page.available = page.available || a_available;
                return;
            }
        }

        MCMPageRow page;
        page.name = a_name;
        page.index = a_index;
        page.available = a_available;
        pages.push_back(std::move(page));
    }

    void MCMPagesWindow::Refresh(bool a_loadChoices)
    {
        // Read saved pages. Exclusion choices are only loaded when opening the window.
        Profile saved;
        const bool profileLoaded = ProfileStorage::Load(profile, saved);
        if (a_loadChoices && profileLoaded) {
            const auto found = saved.pageExclusions.find(identity.modID);
            if (found != saved.pageExclusions.end()) {
                for (const auto& exclusion : found->second) {
                    MCMPageRow page;
                    static_cast<MCMPageExclusion&>(page) = exclusion;
                    pages.push_back(std::move(page));
                }
            }
        }

        for (auto& page : pages) {
            page.available = false;
        }

        if (IsGameLoaded()) {
            for (const auto& mcm : MCMRegistry().ReadRegisteredMCMs()) {

                if (mcm.identity.modID != identity.modID) {
                    continue;
                }

                MCMScript script(mcm.mcmScript);

                std::vector<std::string> names;
                script.ReadPages(names);

                const auto current = script.ReadCurrentPage();
                for (size_t index = 0; index < names.size(); ++index) {
                    AddPage(names[index], static_cast<int>(index), true, std::count(names.begin(), names.end(), names[index]) == 1);
                }

                if (names.empty() && !current) {
                    AddPage("", -1, true);
                }

                if (current && (current->name.empty() || std::find(names.begin(), names.end(), current->name) == names.end())) {
                    AddPage(current->name, current->index, true);
                }

                break;
            }
        }

        if (profileLoaded) {
            for (const auto& setting : saved.settings) {
                if (setting.selection.identity.modID == identity.modID) {
                    AddPage(setting.selection.pageName, setting.selection.pageIndex, false);
                }
            }

            for (const auto& activation : saved.activations) {
                if (activation.selection.identity.modID == identity.modID) {
                    AddPage(activation.selection.pageName, activation.selection.pageIndex, false);
                }
            }
        }
    }

    void MCMPagesWindow::Apply()
    {
        std::vector<MCMPageExclusion> exclusions;
        exclusions.reserve(pages.size());
        for (const auto& page : pages) {
            // Check for duplicate names and mark them to match their index.
            MCMPageExclusion exclusion = page;
            size_t sameNameCount{};
            for (const auto& other : pages) {
                sameNameCount += other.name == page.name ? 1 : 0;
            }
            exclusion.matchIndex = page.matchIndex || page.name.empty() || sameNameCount > 1;
            exclusions.push_back(std::move(exclusion));
        }

        if (ProfileStorage::SavePageExclusions(profile, identity.modID, exclusions)) {
            error.clear();
            open = false;
        }
        else {
            error = "Profile.Pages.SaveFailed";
        }
    }

    void MCMPagesWindow::Render()
    {
        if (!open) {
            return;
        }

        if (profile != GetSettings().activeProfile) {
            open = false;
            return;
        }

        const auto title = std::format("{} - {}###MCM Pages", Trans::Tr("Profile.Pages.Title"), GetDisplayModName(identity.modName));
        
        if (GUI::BeginWindow(title.c_str(), std::addressof(open), 810.0F, 470.0F, true)) {
            const bool busy = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
           
            GUI::BeginDisabled(busy);
            if (GUI::Button(Trans::Tr("Profile.Pages.Refresh").c_str())) {
                Refresh();
            }
            GUI::EndDisabled();
            
            GUI::WrappedTooltip(Trans::Tr("Profile.Pages.Refresh.Tooltip").c_str());
            
            GUI::Spacing();
            
            std::array<std::string, pageExclusionLabels.size()> modeLabels;
            std::array<const char*, pageExclusionLabels.size()> modeItems;
            for (size_t mode = 0; mode < modeLabels.size(); ++mode) {
                modeLabels[mode] = Trans::Tr(pageExclusionLabels[mode]);
                modeItems[mode] = modeLabels[mode].c_str();
            }

            const float height = std::max(GUI::GetFrameHeight() * 3.0F, GUI::GetAvailableHeight() - GUI::GetFrameHeightWithSpacing() * (error.empty() ? 2.0F : 4.0F));
            const auto flags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_ScrollY;
            if (GUI::BeginTable("MCM Pages", 3, flags, GUI::ImVec2{ 0.0F, height })) {

                GUI::TableSetupColumn(Trans::Tr("Profile.Pages.Name").c_str(), GUI::ImGuiTableColumnFlags_WidthStretch);
                GUI::TableSetupColumn(Trans::Tr("Profile.Pages.Status").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 170.0F);
                GUI::TableSetupColumn(Trans::Tr("Profile.Pages.Mode").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 290.0F);

                GUI::TableSetupScrollFreeze(0, 1);

                GUI::TableHeadersRow();
                for (size_t index = 0; index < pages.size(); ++index) {
                    auto& page = pages[index];

                    GUI::PushID(static_cast<int>(index));

                    GUI::TableNextRow();

                    GUI::TableSetColumnIndex(0);
                    GUI::AlignTextToFramePadding();

                    auto label = page.name.empty() ? Trans::Tr("Profile.Pages.Main") : GetDisplayText(page.name);

                    size_t sameNameCount{};
                    for (const auto& other : pages) {
                        sameNameCount += other.name == page.name ? 1 : 0;
                    }
                    if (sameNameCount > 1) {
                        label += std::format(" [{}]", page.index);
                    }

                    GUI::TextUnformatted(label.c_str());

                    GUI::TableSetColumnIndex(1);
                    GUI::AlignTextToFramePadding();

                    GUI::TextUnformatted(Trans::Tr(page.available ? "Profile.Pages.Available" : "Profile.Pages.Saved").c_str());

                    GUI::TableSetColumnIndex(2);

                    GUI::SetNextItemWidth(-1.0F);

                    int mode = static_cast<int>(ToIndex(page.mode));
                    if (GUI::Combo("##Mode", std::addressof(mode), modeItems.data(), static_cast<int>(modeItems.size()))) {
                        page.mode = static_cast<PageExclusionMode>(mode);
                    }

                    GUI::PopID();
                }

                if (pages.empty()) {
                    GUI::TableNextRow();
                    GUI::TableSetColumnIndex(0);
                    GUI::TextWrapped("%s", Trans::Tr("Profile.Pages.Empty").c_str());
                }

                GUI::EndTable();
            }

            if (!error.empty()) {
                GUI::TextWrapped("%s", Trans::Tr(error).c_str());
            }

            const auto applyLabel = Trans::Tr("Profile.Pages.Apply");
            const auto cancelLabel = Trans::Tr("Common.Action.Cancel");
            const float buttonHeight = std::max(GUI::MeasureButton(applyLabel.c_str()).height, GUI::MeasureButton(cancelLabel.c_str()).height);
            GUI::SetCursorPosY(GUI::GetCursorPosY() + std::max(0.0F, GUI::GetAvailableHeight() - buttonHeight));

            if (CTAButton(applyLabel.c_str(), !busy, Color::kCreateButtonColors)) {
                Apply();
            }

            GUI::SameLine(0.0F, 10.0F);

            if (CTAButton(cancelLabel.c_str(), true, Color::kNeutralButtonColors)) {
                open = false;
            }
        }

        GUI::EndWindow();
    }
}

