#include "mcm/mcm_script.hpp"
#include "mcm/mcm_support.hpp"

namespace MCMMemory
{
    // Variable is the value return by Papyrus but we don't care about it.
    // () allows the created object to be called like a function. 
    // See new MCMCallResult in RunNextAction.
    void MCMCallResult::operator()(RE::BSScript::Variable)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !task) {
            logger::error("A completed MCM script call could not reach the game task queue");
            return;
        }
        tasks->AddTask(std::move(task));
    }

    bool MCMScript::Call(std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result) const
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !script) {
            return false;
        }

        auto mcmScript = script;
        return vm->DispatchMethodCall(mcmScript, RE::BSFixedString(a_functionName), a_arguments, a_result);
    }

    bool MCMScript::IsBasedOn(std::string_view a_scriptName) const
    {
        auto* typeInfo = script ? script->GetTypeInfo() : nullptr;
        for (; typeInfo; typeInfo = typeInfo->GetParent()) {
            const auto* name = typeInfo->GetName();
            if (name && a_scriptName == name) {
                return true;
            }
        }
        return false;
    }

    std::optional<MCMPage> MCMScript::ReadCurrentPage() const
    {
        auto pageNumber = ReadInteger("_currentPageNum");
        const auto* pageName = FindVariable("_currentPage");
        if (!IsConfigOpen() || !pageNumber || *pageNumber < 0 || !pageName || !pageName->IsString()) {
            return std::nullopt;
        }

        MCMPage page{ std::string(pageName->GetString()), *pageNumber - 1 };
        if (page.index < 0) {
            return page.name.empty() ? std::optional<MCMPage>(std::move(page)) : std::nullopt;
        }

        auto registeredName = ReadString("Pages", static_cast<size_t>(page.index));
        return registeredName && *registeredName == page.name ? std::optional<MCMPage>(std::move(page)) : std::nullopt;
    }

    const RE::BSScript::Variable* MCMScript::FindVariable(std::string_view a_name) const
    {
        if (!script) {
            return nullptr;
        }

        RE::BSFixedString name(a_name);
        const auto* value = script->GetVariable(name);
        if (!value) {
            value = script->GetProperty(name);
        }
        if (!value) {
            const auto backingName = std::format("::{}_var", a_name);
            value = script->GetVariable(RE::BSFixedString(backingName));
        }
        return value;
    }

    std::optional<float> MCMScript::ReadNumber(std::string_view a_name, size_t a_index) const
    {
        auto values = ReadArray(a_name);
        if (!values || a_index >= values->size()) {
            return std::nullopt;
        }

        const auto& value = (*values)[static_cast<uint32_t>(a_index)];
        if (value.IsFloat()) {
            return value.GetFloat();
        }
        if (value.IsInt()) {
            return static_cast<float>(value.GetSInt());
        }
        if (value.IsBool()) {
            return value.GetBool() ? 1.0F : 0.0F;
        }
        return std::nullopt;
    }

    std::optional<std::string> MCMScript::ReadString(std::string_view a_name, size_t a_index) const
    {
        auto values = ReadArray(a_name);
        if (!values || a_index >= values->size()) {
            return std::nullopt;
        }

        const auto& value = (*values)[static_cast<uint32_t>(a_index)];
        return value.IsString() ? std::optional<std::string>(std::string(value.GetString())) : std::nullopt;
    }

    std::vector<std::string> MCMScript::ReadPages() const
    {
        std::vector<std::string> pages;
        if (!script) {
            return pages;
        }

        const auto* value = script->GetProperty("Pages");
        if (!value || !value->IsArray()) {
            value = script->GetVariable("::Pages_var");
        }
        auto pageArray = value && value->IsArray() ? value->GetArray() : RE::BSTSmartPointer<RE::BSScript::Array>();
        if (!pageArray) {
            return pages;
        }

        pages.reserve(pageArray->size());
        for (const auto& page : *pageArray) {
            if (page.IsString()) {
                pages.emplace_back(page.GetString());
            }
        }
        return pages;
    }

    bool MCMScript::ReadPage(const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, std::vector<CapturedSetting>& a_settings) const
    {
        auto flags = ReadArray("_optionFlagsBuf");
        auto labels = ReadArray("_textBuf");
        auto numbers = ReadArray("_numValueBuf");
        auto strings = ReadArray("_strValueBuf");
        auto states = ReadArray("_stateOptionMap");
        if (!flags || !labels || !numbers || !strings) {
            return false;
        }

        const size_t optionCount = std::min({ static_cast<size_t>(flags->size()), static_cast<size_t>(labels->size()), static_cast<size_t>(numbers->size()), static_cast<size_t>(strings->size()) });
        const bool pageScopedState = NLMCMSupport::IsSupported(*this);
        for (size_t optionIndex = 0; optionIndex < optionCount; ++optionIndex) {
            const auto& flagValue = (*flags)[static_cast<uint32_t>(optionIndex)];
            if (!flagValue.IsInt()) {
                continue;
            }

            const int optionFlags = flagValue.GetSInt();
            const int skyUIType = optionFlags % 256;
            const int flagsOnly = optionFlags / 256;
            if ((flagsOnly & 2) != 0 || skyUIType < 0 || static_cast<size_t>(skyUIType) >= skyUIControlTypes.size()) {
                continue;
            }

            auto label = ReadString("_textBuf", optionIndex);
            if (!label || label->empty()) {
                continue;
            }

            CapturedSetting setting;
            setting.pageScopedState = pageScopedState;
            setting.selection.identity = a_identity;
            setting.selection.pageName = a_pageName;
            setting.selection.pageIndex = a_pageIndex;
            setting.selection.optionIndex = static_cast<int>(optionIndex);
            setting.optionLabel = std::move(*label);
            setting.type = skyUIControlTypes[static_cast<size_t>(skyUIType)];
            if (setting.type == ControlType::Unknown) {
                continue;
            }
            if (states && optionIndex < states->size()) {
                const auto& state = (*states)[static_cast<uint32_t>(optionIndex)];
                if (state.IsString()) {
                    setting.stateName = state.GetString();
                }
            }

            switch (skyUIType) {
            case 3:
                setting.type = ControlType::Option;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = *value != 0.0F;
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case 4:
                setting.type = ControlType::Slider;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = *value;
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case 5:
                setting.type = ControlType::Menu;
                break;
            case 6:
                setting.type = ControlType::Color;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = static_cast<int>(*value);
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case 7:
                setting.type = ControlType::Keymap;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = static_cast<int>(*value);
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case 8:
                setting.type = ControlType::Input;
                if (auto value = ReadString("_strValueBuf", optionIndex)) {
                    setting.value = std::move(*value);
                    setting.valueSource = "script._strValueBuf";
                }
                break;
            default:
                break;
            }

            setting.identityComplete = setting.type != ControlType::Unknown && (setting.type == ControlType::Menu || !setting.value.is_null());
            if (setting.identityComplete) {
                a_settings.push_back(std::move(setting));
            }
        }
        return true;
    }

    std::optional<nlohmann::json> MCMScript::ReadCurrentValue(ControlType a_type, int a_optionIndex) const
    {
        if (a_optionIndex < 0) {
            return std::nullopt;
        }

        const size_t index = static_cast<size_t>(a_optionIndex);
        switch (a_type) {
        case ControlType::Option:
            if (auto value = ReadNumber("_numValueBuf", index)) {
                return nlohmann::json(*value != 0.0F);
            }
            break;
        case ControlType::Slider:
            if (auto value = ReadNumber("_numValueBuf", index)) {
                return nlohmann::json(*value);
            }
            break;
        case ControlType::Color:
        case ControlType::Keymap:
            if (auto value = ReadNumber("_numValueBuf", index)) {
                return nlohmann::json(static_cast<int>(*value));
            }
            break;
        case ControlType::Input:
            if (auto value = ReadString("_strValueBuf", index)) {
                return nlohmann::json(std::move(*value));
            }
            break;
        default:
            break;
        }
        return std::nullopt;
    }

    std::optional<int> MCMScript::ReadMenuIndex() const
    {
        auto value = ReadNumber("_menuParams", 0);
        return value ? std::optional<int>(static_cast<int>(*value)) : std::nullopt;
    }

    std::optional<std::string> MCMScript::ReadStateName(int a_optionIndex) const
    {
        return a_optionIndex >= 0 ? ReadString("_stateOptionMap", static_cast<size_t>(a_optionIndex)) : std::nullopt;
    }

    std::optional<ControlType> MCMScript::ReadControlType(int a_optionIndex) const
    {
        if (a_optionIndex < 0) {
            return std::nullopt;
        }

        const size_t index = static_cast<size_t>(a_optionIndex);
        auto flag = ReadNumber("_optionFlagsBuf", index);
        if (!flag) {
            return std::nullopt;
        }

        const int skyUIType = static_cast<int>(*flag) % 256;
        if (skyUIType < 0 || static_cast<size_t>(skyUIType) >= skyUIControlTypes.size()) {
            return std::nullopt;
        }
        return skyUIControlTypes[static_cast<size_t>(skyUIType)];
    }

    bool MCMScript::MatchesControl(ControlType a_type, int a_optionIndex, std::string_view a_stateName) const
    {
        if (ReadControlType(a_optionIndex) != a_type) {
            return false;
        }

        if (!a_stateName.empty()) {
            auto stateName = ReadStateName(a_optionIndex);
            return stateName && *stateName == a_stateName;
        }
        return true;
    }

    std::optional<MCMControl> MCMScript::ReadControl(int a_optionIndex) const
    {
        auto type = ReadControlType(a_optionIndex);
        auto label = ReadOptionLabel(a_optionIndex);
        if (!type || !label || label->empty()) {
            return std::nullopt;
        }

        MCMControl control;
        control.optionLabel = std::move(*label);
        control.stateName = ReadStateName(a_optionIndex).value_or("");
        control.type = *type;
        return control;
    }

    std::optional<int> MCMScript::FindControlIndex(const MCMControl& a_control, int a_previousIndex) const
    {
        if (a_control.stateName.empty()) {
            // Without a state name, do not guess which similar rows moved.
            auto label = ReadOptionLabel(a_previousIndex);
            return MatchesControl(a_control.type, a_previousIndex, "") && label && *label == a_control.optionLabel ? std::optional<int>(a_previousIndex) : std::nullopt;
        }

        auto flags = ReadArray("_optionFlagsBuf");
        if (!flags) {
            return std::nullopt;
        }

        std::optional<int> result;
        for (uint32_t index = 0; index < flags->size(); ++index) {
            if (MatchesControl(a_control.type, static_cast<int>(index), a_control.stateName)) {
                if (result) {
                    return std::nullopt;
                }
                result = static_cast<int>(index);
            }
        }
        return result;
    }

    bool MCMScript::IsConfigOpen() const
    {
        auto flags = ReadArray("_optionFlagsBuf");
        auto state = ReadInteger("_state");
        return flags && flags->size() >= 128 && state && *state == 0;
    }

    bool MCMScript::IsPageReady(int a_pageIndex) const
    {
        auto currentPage = ReadInteger("_currentPageNum");
        auto state = ReadInteger("_state");
        return currentPage && *currentPage == a_pageIndex + 1 && state && *state == 0;
    }

    bool MCMScript::IsMenuReady(int a_optionIndex) const
    {
        auto currentPage = ReadInteger("_currentPageNum");
        auto activeOption = ReadInteger("_activeOption");
        auto state = ReadInteger("_state");
        return currentPage && activeOption && *activeOption == a_optionIndex + *currentPage * 256 && state && *state == 0;
    }
}
