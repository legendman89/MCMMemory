#include "menu/pages.hpp"
#include "menu/translate.hpp"
#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
#include "profile/profile.hpp"
#include "profile/backup.hpp"
#include "profile/restore.hpp"
#include "settings.hpp"

namespace MCMMemory::Menu
{
    void MCMPagesWindow::Open(const MCMIdentity& a_identity)
    {
        identity = a_identity;
        profile = GetSettings().activeProfile;
        pages.clear();
        if (const auto* saved = FindChoices()) {
            pages = saved->pages;
        }
        open = true;
        Refresh();
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
        if (!a_name.empty() && nameMatches == 1 && (a_uniqueName || (!a_available && namedPage->available))) {
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

    void MCMPagesWindow::Refresh()
    {
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

        Profile saved;
        if (ProfileStorage::Load(profile, saved)) {
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

    MCMPageChoices* MCMPagesWindow::FindChoices()
    {
        for (auto& saved : choices) {
            if (saved.profile == profile && saved.modID == identity.modID) {
                return std::addressof(saved);
            }
        }

        return nullptr;
    }

    void MCMPagesWindow::Apply()
    {
        if (auto* saved = FindChoices()) {
            saved->pages = pages;
            open = false;
            return;
        }

        MCMPageChoices saved;
        saved.profile = profile;
        saved.modID = identity.modID;
        saved.pages = pages;
        choices.push_back(std::move(saved));
        open = false;
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

        GUI::SetNextWindowSize(GUI::ImVec2{ 770.0F, 460.0F }, GUI::ImGuiCond_FirstUseEver);

        CenterNextWindow();

        const auto title = std::format("{} - {}###MCM Pages", Trans::Tr("Profile.Pages.Title"), GetDisplayModName(identity.modName));
        if (GUI::Begin(title.c_str(), std::addressof(open), GUI::ImGuiWindowFlags_NoCollapse)) {
            const bool busy = Backup::GetSingleton()->GetStatus() != OperationStatus::Idle || Restore::GetSingleton()->GetStatus() != OperationStatus::Idle;
           
            GUI::BeginDisabled(busy);
            if (GUI::Button(Trans::Tr("Profile.Pages.Refresh").c_str())) {
                Refresh();
            }
            GUI::EndDisabled();
            
            WrappedTooltip(Trans::Tr("Profile.Pages.Refresh.Tooltip").c_str());
            
            GUI::Spacing();
            
            const float height = std::max(GUI::GetFrameHeight() * 3.0F, GUI::GetContentRegionAvail().y - GUI::GetFrameHeightWithSpacing() * 2.0F);
            const auto flags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_ScrollY;
            if (GUI::BeginTable("MCM Pages", 3, flags, GUI::ImVec2{ 0.0F, height })) {

                GUI::TableSetupColumn(Trans::Tr("Profile.Pages.Name").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 300.0F);
                GUI::TableSetupColumn(Trans::Tr("Profile.Pages.Status").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 150.0F);
                GUI::TableSetupColumn(Trans::Tr("Profile.Pages.Mode").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 260.0F);

                GUI::TableSetupScrollFreeze(0, 1);

                GUI::TableHeadersRow();
                for (size_t index = 0; index < pages.size(); ++index) {
                    auto& page = pages[index];

                    GUI::PushID(static_cast<int>(index));

                    GUI::TableNextRow();

                    GUI::TableSetColumnIndex(0);

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

                    GUI::TextUnformatted(Trans::Tr(page.available ? "Profile.Pages.Available" : "Profile.Pages.Saved").c_str());

                    GUI::TableSetColumnIndex(2);

                    GUI::SetNextItemWidth(-1.0F);

                    const auto preview = Trans::Tr(pageExclusionLabels[ToIndex(page.mode)]);
                    if (BeginOpaqueCombo("##Mode", preview.c_str())) {
                        for (size_t mode = 0; mode < pageExclusionLabels.size(); ++mode) {
                            if (GUI::Selectable(Trans::Tr(pageExclusionLabels[mode]).c_str(), ToIndex(page.mode) == mode)) {
                                page.mode = static_cast<PageExclusionMode>(mode);
                            }
                        }
                        GUI::EndCombo();
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

            if (CTAButton(Trans::Tr("Profile.Pages.Apply").c_str(), !busy, Color::kCreateButtonColors)) {
                Apply();
            }

            GUI::SameLine(0.0F, 10.0F);

            if (CTAButton(Trans::Tr("Common.Action.Cancel").c_str(), true, Color::kNeutralButtonColors)) {
                open = false;
            }
        }

        GUI::End();
    }
}

