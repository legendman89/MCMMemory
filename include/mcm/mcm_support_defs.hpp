#pragma once

// Add stable page keys here when an MCM page only manages external configurations.
#define FOREACH_IGNORED_MCM_PAGE(IGNORED_PAGE) \
    IGNORED_PAGE("SaveLoad") \
    IGNORED_PAGE("ImportExport")

// These words only ignore text buttons. Normal toggles, sliders and menus are unaffected.
#define FOREACH_IGNORED_MCM_COMMAND(IGNORED_COMMAND) \
    IGNORED_COMMAND("Save") \
    IGNORED_COMMAND("Load") \
    IGNORED_COMMAND("Delete") \
    IGNORED_COMMAND("Import") \
    IGNORED_COMMAND("Export") \
    IGNORED_COMMAND("Rename") \
    IGNORED_COMMAND("Duplicate") \
    IGNORED_COMMAND("Reset")

// Ignore commands in NFF from backup/restore.
#define FOREACH_IGNORED_MCM_CONTROL(IGNORED_CONTROL) \
    IGNORED_CONTROL("nwsfollowermcmscript", 9, ControlType::Menu, "$FF_selBase") \
    IGNORED_CONTROL("nwsfollowermcmscript", 9, ControlType::Input, "Rename Base")

// SkyUI text controls whose selected values are stored as integers in their configuration scripts.
// Source: VL_ConfigMenu.psc
#define FOREACH_SKYUI_SCRIPT_CYCLE_SETTING(CYCLE_SETTING) \
    CYCLE_SETTING("VL_ConfigMenu", 0, "KillmoveOID", "KillmoveListIndex", "$Killmoves", "", 4, false) \
    CYCLE_SETTING("VL_ConfigMenu", 0, "RangedModeOID", "RangedModeListIndex", "$Selection Mode", "", 2, false) \
    CYCLE_SETTING("VL_ConfigMenu", 0, "MoveAnimationsOID", "MoveAnimationsListIndex", "$Advancing Killmoves", "", 3, false) \
    CYCLE_SETTING("VL_ConfigMenu", 0, "DecapitationOID", "DecapitationListIndex", "$Decapitations", "", 3, false) \
    CYCLE_SETTING("VL_ConfigMenu", 0, "RangedKillmoveOID", "RangedKillmoveListIndex", "$Killmoves", "", 3, false) \
    CYCLE_SETTING("VL_ConfigMenu", 0, "RangedPerspectiveOID", "RangedPerspectiveListIndex", "$Camera View", "RangedPerspectiveList", 2, true)

// Better Vampire patch providingSkyUI text controls whose current values are stored in Skyrim GlobalVariable properties.
// Source: BetterVampiresConfigMenu.psc
#define BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, optionVariable, valueVariable, profileLabel, valueCount) \
    CYCLE_SETTING("BetterVampiresConfigMenu", 4, optionVariable, valueVariable, "", profileLabel, valueCount, 0, 10000)

#define FOREACH_SKYUI_GLOBAL_CYCLE_SETTING(CYCLE_SETTING, CUSTOM_CYCLE_SETTING) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_StageProgressionOID_T", "VampireProgression", "Stage Progression", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_StageTimingOID_T", "VampireDynamicStages", "Stage Timing", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_RankProgressionOID_T", "VampireRankProgression", "Rank Progression", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VampireBloodPointsOID_T", "EnableVampireBloodPoints", "Blood Points", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_StageAbilitiesSatiationOID_T", "VampireStageAbilitiesSatiation", "Stage Ability Satiation", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_RankAbilitiesSatiationOID_T", "VampireRankAbilitiesSatiation", "Rank Ability Satiation", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_StageHatedOID_T", "VampireStageHated", "Hated Stage", 4) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_BetterVampiresDamageOID_T", "BetterVampiresDamage", "Spell Damage", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_BetterVampiresDamageMeleeOID_T", "BetterVampiresDamageMelee", "Melee Damage", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_UsingVampireEnthrallPerkOID_T", "UsingVampireEnthrallPerk", "Vampire Cattle", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_FeedOffDeadOID_T", "VampireFeedOffDead", "Feed Off Dead", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_SneakFeedOID_T", "VampireSneakFeed", "Sneak Feed", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_ForceFeedOID_T", "VampireForceFeed", "Force Feed", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_CombatBiteOID_T", "VampireCombatBite", "Combat Bite", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VampireFeedingAnimationOID_T", "VampireFeedingAnimation", "Feeding Animation", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_ExtractBloodOID_T", "VampireExtractBlood", "Extract Blood", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_SunEffectsOID_T", "VampireSunEffects", "Sun Effects", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_LightLevelPenaltiesOID_T", "VampireLightLevelPenalties", "Light Level Penalties", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_LightLevelRegenOID_T", "VampireLightLevelRegen", "Light Level Regeneration", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_SunDamageOID_T", "VampireSunDamage", "Sun Damage", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_SunDamageSpecialOID_T", "VampireSunDamageSpecial", "Specialized Sun Damage", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VictimAppearanceOID_T", "VampireVictimAppearance", "Victim Appearance", 3) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VictimSkillsOID_T", "VampireVictimSkills", "Victim Skills", 2) \
    CUSTOM_CYCLE_SETTING("BetterVampiresConfigMenu", 4, "_HuntersOID_T", "VampireHunters", "", "Vampire Hunters", 6, 0, 10000, 20000, 30000, 40000, 100000) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_BVSpecialVictimFeedingOID_T", "BVSpecialVictimFeeding", "Special Feeding Victims", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VampireAmaranthFeedOID_T", "VampireAmaranthFeed", "Amaranth Feeding", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VampirePraeceptorPerksOID_T", "VampirePraeceptorPerks", "Praeceptor Perks", 2) \
    BETTER_VAMPIRES_GLOBAL_CYCLE(CYCLE_SETTING, "_VampireEngorgeOID_T", "VampireEngorge", "Engorge", 2) \
    CYCLE_SETTING("BetterVampiresConfigMenu", 5, "_VampireBloodMeterDisplay_ContextualOID_T", "BetterVampiresBloodMeterDisplay_Contextual", "", "Blood Meter Display", 2, 1, 1)

// An activation control needs a status label and one of these enabled/disabled value pairs.
// I'm still not satisfied as Enable/Disable might conflict with actual toggle controls.
#define FOREACH_MCM_ACTIVATION_LABEL(ACTIVATION_LABEL) \
    ACTIVATION_LABEL("Status") \
    ACTIVATION_LABEL("State") \
    ACTIVATION_LABEL("Enable") \
    ACTIVATION_LABEL("Disable") \
    ACTIVATION_LABEL("Activation") \
    ACTIVATION_LABEL("Start")

#define FOREACH_MCM_ACTIVATION_STATUS(ACTIVATION_STATUS) \
    ACTIVATION_STATUS("Enabled", "Disabled") \
    ACTIVATION_STATUS("Running", "Stopped") \
    ACTIVATION_STATUS("Active", "Inactive")

// Text buttons can expose the current activation state by changing their command verb.
#define FOREACH_MCM_ACTIVATION_COMMAND(ACTIVATION_COMMAND) \
    ACTIVATION_COMMAND("Start", "Stop") \
    ACTIVATION_COMMAND("Activate", "Deactivate") \
    ACTIVATION_COMMAND("Enable", "Disable")
