#pragma once

#include "profile/types.hpp"

namespace MCMMemory
{
    // SkyUI stores the control type in the low byte of each option flag.
    inline constexpr std::array<ControlType, 9> skyUIControlTypes
    {
        ControlType::Unknown,
        ControlType::Unknown,
        ControlType::Unknown,
        ControlType::Option,
        ControlType::Slider,
        ControlType::Menu,
        ControlType::Color,
        ControlType::Keymap,
        ControlType::Input
    };

    struct MCMCallResult : public RE::BSScript::IStackCallbackFunctor
    {
        // Runs the next backup or restore step after the Papyrus call finishes.
        SKSE::TaskInterface::TaskFn task;

        explicit MCMCallResult(SKSE::TaskInterface::TaskFn a_task) : task(std::move(a_task)) {}

        void operator()(RE::BSScript::Variable a_result) override;

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    class MCMScript
    {
    public:

        explicit MCMScript(RE::BSTSmartPointer<RE::BSScript::Object> a_script) : script(std::move(a_script)) {}

        // Calls an MCM function asynchronously through the Papyrus VM.
        bool Call(std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result = {}) const;

        std::vector<std::string> ReadPages() const;

        // Includes inherited script types, not just the mod's own script name.
        bool IsBasedOn(std::string_view a_scriptName) const;

        std::optional<MCMPage> ReadCurrentPage() const;

        // Reads the controls SkyUI prepared for the currently loaded page.
        bool ReadPage(const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, std::vector<CapturedSetting>& a_settings) const;

        std::optional<nlohmann::json> ReadCurrentValue(ControlType a_type, int a_optionIndex) const;

        std::optional<int> ReadMenuIndex() const;

        std::optional<std::string> ReadStateName(int a_optionIndex) const;

        std::optional<MCMControl> ReadControl(int a_optionIndex) const;

        // A state-based toggle can move to another row after a page reset.
        // Some mods clear the page to hide disabled controls.
        std::optional<int> FindControlIndex(const MCMControl& a_control, int a_previousIndex) const;

        // Confirms the saved setting still points to the same live control before restore.
        bool MatchesControl(ControlType a_type, int a_optionIndex, std::string_view a_stateName) const;

        // These checks stop us from reading SkyUI buffers while it is still updating them.
        bool IsConfigOpen() const;

        bool IsPageReady(int a_pageIndex) const;

        bool IsMenuReady(int a_optionIndex) const;

        inline std::optional<std::string> ReadOptionLabel(int a_optionIndex) const
        {
            return a_optionIndex >= 0 ? ReadString("_textBuf", static_cast<size_t>(a_optionIndex)) : std::nullopt;
        }

    private:

        // Supports normal variables, properties, and their generated Papyrus backing names.
        const RE::BSScript::Variable* FindVariable(std::string_view a_name) const;

        inline RE::BSTSmartPointer<RE::BSScript::Array> ReadArray(std::string_view a_name) const
        {
            const auto* value = FindVariable(a_name);
            return value && value->IsArray() ? value->GetArray() : RE::BSTSmartPointer<RE::BSScript::Array>();
        }

        std::optional<float> ReadNumber(std::string_view a_name, size_t a_index) const;

        std::optional<std::string> ReadString(std::string_view a_name, size_t a_index) const;

        std::optional<ControlType> ReadControlType(int a_optionIndex) const;

        inline std::optional<int> ReadInteger(std::string_view a_name) const
        {
            const auto* value = FindVariable(a_name);
            return value && value->IsInt() ? std::optional<int>(value->GetSInt()) : std::nullopt;
        }

        // Keeps the live config script alive while backup or restore is using it.
        RE::BSTSmartPointer<RE::BSScript::Object> script;
    };
}
