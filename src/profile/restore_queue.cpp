#include "profile/restore.hpp"
#include "mcm/mcm_support.hpp"

namespace MCMMemory
{
    void Restore::MatchRegisteredMCMs(const std::vector<MCMRegistryEntry>& a_registeredMCMs)
    {
        // Match saved stable IDs to the live MCM scripts.
        for (auto& mcm : restoreMCMs) {
            mcm.mcmScript = {};
            for (const auto& registeredMCM : a_registeredMCMs) {
                if (registeredMCM.identity.modID == mcm.identity.modID) {
                    mcm.mcmScript = registeredMCM.mcmScript;
                    logger::debug("Found profile MCM '{}' as '{}' in the active registry", mcm.identity.modID, registeredMCM.identity.modName);
                    break;
                }
            }
            if (!mcm.mcmScript) {
                logger::warn("Skipping unavailable profile MCM '{}'", mcm.identity.modID);
            }
        }
    }

    void Restore::BuildActionQueue()
    {
        actions.clear();

        // Reserve once because the final queue size is already known.
        size_t actionCount{};
        for (const auto& mcm : restoreMCMs) {
            if (mcm.mcmScript) {
                actionCount += mcm.settingActions.size() + 2; // +2 for open and close configs.
                if (mcm.activation) {
                    // We need 6 actions implemented below.
                    // This should cover most of the MCMs.
                    actionCount += 6;
                }
            }
        }
        actions.reserve(actionCount);

        // Open each MCM once, run its settings, then close it.
        for (size_t mcmIndex = 0; mcmIndex < restoreMCMs.size(); ++mcmIndex) {
            const auto& mcm = restoreMCMs[mcmIndex];
            if (!mcm.mcmScript) {
                continue;
            }

            if (mcm.activation) {
                auto open = MakeRestoreAction(RestoreActionType::OpenConfig, mcmIndex);
                open.activationStep = true;
                actions.push_back(std::move(open));

                if (mcm.activation->selection.pageIndex >= 0) {
                    auto page = MakePageAction(mcmIndex, mcm.activation->selection);
                    page.activationStep = true;
                    actions.push_back(std::move(page));
                }

                auto activate = MakeRestoreAction(RestoreActionType::ActivateMCM, mcmIndex);
                activate.activationStep = true;
                actions.push_back(std::move(activate));

                auto close = MakeRestoreAction(RestoreActionType::CloseConfig, mcmIndex);
                close.activationStep = true;
                actions.push_back(std::move(close));
            }

            actions.push_back(MakeRestoreAction(RestoreActionType::OpenConfig, mcmIndex));
            if (mcm.activation) {
                if (mcm.activation->selection.pageIndex >= 0) {
                    auto page = MakePageAction(mcmIndex, mcm.activation->selection);
                    page.activationStep = true;
                    actions.push_back(std::move(page));
                }

                auto verify = MakeRestoreAction(RestoreActionType::VerifyMCM, mcmIndex);
                verify.activationStep = true;
                actions.push_back(std::move(verify));
            }
            actions.insert(actions.end(), mcm.settingActions.begin(), mcm.settingActions.end());
            actions.push_back(MakeRestoreAction(RestoreActionType::CloseConfig, mcmIndex));
        }
    }
}
