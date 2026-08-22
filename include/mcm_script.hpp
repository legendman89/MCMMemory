#pragma once

#include "types.hpp"

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

        // Reads the controls SkyUI prepared for the currently loaded page.
        bool ReadPage(const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, std::vector<CapturedSetting>& a_settings) const;

        std::optional<nlohmann::json> ReadCurrentValue(ControlType a_type, int a_optionIndex) const;

        std::optional<int> ReadMenuIndex() const;

        std::optional<std::string> ReadStateName(int a_optionIndex) const;

        // Confirms the saved setting still points to the same live control before restore.
        bool MatchesControl(ControlType a_type, int a_optionIndex, std::string_view a_stateName) const;

        // These checks stop us from reading SkyUI buffers while it is still updating them.
        bool IsConfigOpen() const;

        bool IsPageReady(int a_pageIndex) const;

        bool IsMenuReady(int a_optionIndex) const;

    private:

        // Supports normal variables, properties, and their generated Papyrus backing names.
        const RE::BSScript::Variable* FindVariable(std::string_view a_name) const;

        RE::BSTSmartPointer<RE::BSScript::Array> ReadArray(std::string_view a_name) const;

        std::optional<float> ReadNumber(std::string_view a_name, size_t a_index) const;

        std::optional<std::string> ReadString(std::string_view a_name, size_t a_index) const;

        std::optional<int> ReadInteger(std::string_view a_name) const;

        // Keeps the live config script alive while backup or restore is using it.
        RE::BSTSmartPointer<RE::BSScript::Object> script;
    };
}
