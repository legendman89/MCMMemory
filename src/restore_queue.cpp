#include "restore.hpp"

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
                    logger::info("Found profile MCM '{}' as '{}' using marker {:08X}", mcm.identity.modID, registeredMCM.identity.modName, registeredMCM.markerFormID);
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
            }
        }
        actions.reserve(actionCount);

        // Open each MCM once, run its settings, then close it.
        for (size_t mcmIndex = 0; mcmIndex < restoreMCMs.size(); ++mcmIndex) {
            const auto& mcm = restoreMCMs[mcmIndex];
            if (!mcm.mcmScript) {
                continue;
            }

            actions.push_back(MakeRestoreAction(RestoreActionType::OpenConfig, mcmIndex));
            actions.insert(actions.end(), mcm.settingActions.begin(), mcm.settingActions.end());
            actions.push_back(MakeRestoreAction(RestoreActionType::CloseConfig, mcmIndex));
        }
    }
}
