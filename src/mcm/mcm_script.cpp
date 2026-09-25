#include "mcm/mcm_script.hpp"
#include "mcm/mcm_support.hpp"

namespace MCMMemory
{
    bool MCMScript::Call(std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result) const
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !script) {
            delete a_arguments;
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

        auto pages = ReadPageArray();
        if (!pages || static_cast<size_t>(page.index) >= pages->size()) {
            return std::nullopt;
        }
        const auto& registeredName = (*pages)[static_cast<uint32_t>(page.index)];
        return registeredName.IsString() && page.name == registeredName.GetString() ? std::optional<MCMPage>(std::move(page)) : std::nullopt;
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

    std::optional<float> MCMScript::ReadGlobalValue(std::string_view a_name) const
    {
        const auto* value = FindVariable(a_name);
        auto globalObject = value && value->IsObject() ? value->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        auto* form = policy && globalObject ? policy->GetObjectForHandle(RE::FormType::Global, globalObject->GetHandle()) : nullptr;
        auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
        return global ? std::optional<float>(global->value) : std::nullopt;
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

    RE::BSTSmartPointer<RE::BSScript::Array> MCMScript::ReadPageArray() const
    {
        if (!script) {
            return {};
        }

        const auto* value = script->GetProperty("Pages");
        if (!value || !value->IsArray()) {
            value = script->GetVariable("::Pages_var");
        }
        return value && value->IsArray() ? value->GetArray() : RE::BSTSmartPointer<RE::BSScript::Array>();
    }

    void MCMScript::ReadPages(std::vector<std::string>& a_pages) const
    {
        a_pages.clear();
        auto pageArray = ReadPageArray();
        if (!pageArray) {
            return;
        }

        a_pages.reserve(pageArray->size());
        for (const auto& page : *pageArray) {
            if (page.IsString()) {
                a_pages.emplace_back(page.GetString());
            }
        }
    }

    bool MCMScript::ReadPage(const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, std::vector<CapturedSetting>& a_settings) const
    {
        if (MCMCommandSupport::IsExcludedPage(a_identity.modID, a_pageName, a_pageIndex) || MCMCommandSupport::IsIgnoredPage(a_pageName)) {
            logger::debug("Ignoring configuration management page '{}' in '{}'", a_pageName, a_identity.modID);
            return true;
        }

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
            if (!flagValue.IsInt() || flagValue.GetSInt() < 0) {
                continue;
            }

            const auto optionFlags = static_cast<uint32_t>(flagValue.GetSInt());
            const auto skyUIType = GET_TYPE_FROM_FLAGS(optionFlags);
            const auto flagsOnly = GET_FLAGS_ONLY(optionFlags);
            if ((flagsOnly & 2) != 0 || skyUIType < SkyUIOptionType::Empty || skyUIType >= SkyUIOptionType::Count) {
                continue;
            }

            auto label = ReadString("_textBuf", optionIndex);
            if (!label) {
                continue;
            }

            const auto controlType = skyUIControlTypes[ToIndex(skyUIType)];
            MCMRowLabel rowLabel;
            if (label->empty() && CanUseRowLabel(controlType)) {
                rowLabel = ReadRowLabel(static_cast<int>(optionIndex));
            }
            if (label->empty() && rowLabel.label.empty() && (skyUIType != SkyUIOptionType::Text || !SkyUICycleSupport::Find(*this, static_cast<int>(optionIndex)))) {
                continue;
            }

            CapturedSetting setting;
            setting.pageScopedState = pageScopedState;
            setting.selection.identity = a_identity;
            setting.selection.pageName = a_pageName;
            setting.selection.pageIndex = a_pageIndex;
            setting.selection.optionIndex = static_cast<int>(optionIndex);
            setting.optionLabel = std::move(*label);
            setting.rowLabel = std::move(rowLabel);
            setting.type = controlType;
            if (setting.type == ControlType::Unknown && skyUIType != SkyUIOptionType::Text) {
                continue;
            }
            if (states && optionIndex < states->size()) {
                const auto& state = (*states)[static_cast<uint32_t>(optionIndex)];
                if (state.IsString()) {
                    setting.stateName = state.GetString();
                }
            }
            if (MCMCommandSupport::IsIgnored(a_identity.modID, a_pageName, a_pageIndex, setting.type, setting.stateName, setting.optionLabel)) {
                continue;
            }

            switch (skyUIType) {
            case SkyUIOptionType::Text:
                // Only known cycling controls are safe to discover without a recorded click.
                SkyUICycleSupport::ReadSetting(*this, setting);
                break;
            case SkyUIOptionType::Toggle:
                setting.type = ControlType::Option;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = *value != 0.0F;
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case SkyUIOptionType::Slider:
                setting.type = ControlType::Slider;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = *value;
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case SkyUIOptionType::Menu:
                setting.type = ControlType::Menu;
                break;
            case SkyUIOptionType::Color:
                setting.type = ControlType::Color;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = static_cast<int>(*value);
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case SkyUIOptionType::Keymap:
                setting.type = ControlType::Keymap;
                if (auto value = ReadNumber("_numValueBuf", optionIndex)) {
                    setting.value = static_cast<int>(*value);
                    setting.valueSource = "script._numValueBuf";
                }
                break;
            case SkyUIOptionType::Input:
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
        case ControlType::Cycle:
            if (const auto* cycle = SkyUICycleSupport::Find(*this, a_optionIndex)) {
                if (auto value = SkyUICycleSupport::ReadValue(*this, *cycle)) {
                    return nlohmann::json(*value);
                }
            }
            break;
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

    bool MCMScript::IsTextControl(int a_optionIndex) const
    {
        if (a_optionIndex < 0) {
            return false;
        }
        auto flag = ReadNumber("_optionFlagsBuf", static_cast<size_t>(a_optionIndex));
        return flag && *flag >= 0 && GET_TYPE_FROM_FLAGS(static_cast<uint32_t>(*flag)) == SkyUIOptionType::Text;
    }

    std::optional<uint64_t> MCMScript::ReadPageHash() const
    {
        auto flags = ReadArray("_optionFlagsBuf");
        auto labels = ReadArray("_textBuf");
        if (!flags || !labels) {
            return std::nullopt;
        }

        const size_t optionCount = std::min<size_t>(flags->size(), labels->size());
        uint64_t hash{ hashBasis };
        // We hash the flags and labels, so a changed value leaves 
        // the hash untouched and only a rebuilt page mutates it.
        for (uint32_t optionIndex = 0; optionIndex < optionCount; ++optionIndex) {
            const auto& flagValue = (*flags)[optionIndex];
            AddToHash(hash, flagValue.IsInt() ? static_cast<uint32_t>(flagValue.GetSInt()) : 0);
            const auto& labelValue = (*labels)[optionIndex];
            if (labelValue.IsString()) {
                AddTextToHash(hash, labelValue.GetString());
            }
            // Row separator, so neighbouring labels can't merge.
            AddToHash(hash, 0);
        }
        return hash;
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
        if (!flag || *flag < 0) {
            return std::nullopt;
        }

        const auto skyUIType = GET_TYPE_FROM_FLAGS(static_cast<uint32_t>(*flag));
        if (skyUIType == SkyUIOptionType::Text && SkyUICycleSupport::Find(*this, a_optionIndex)) {
            return ControlType::Cycle;
        }
        if (skyUIType < SkyUIOptionType::Empty || skyUIType >= SkyUIOptionType::Count) {
            return std::nullopt;
        }
        return skyUIControlTypes[ToIndex(skyUIType)];
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

    MCMRowLabel MCMScript::ReadRowLabel(int a_optionIndex) const
    {
        auto labels = ReadArray("_textBuf");
        if (!labels || a_optionIndex < 0 || static_cast<uint32_t>(a_optionIndex) >= labels->size()) {
            return {};
        }

        int distance = 1;
        for (int index = a_optionIndex - skyUIColumnCount; index >= 0; index -= skyUIColumnCount) {
            const auto& label = (*labels)[static_cast<uint32_t>(index)];
            if (label.IsString() && !label.GetString().empty()) {
                return MCMRowLabel{ std::string(label.GetString()), distance };
            }
            ++distance;
        }

        return {};
    }

    bool MCMScript::MatchesLabel(int a_optionIndex, std::string_view a_optionLabel, const MCMRowLabel& a_rowLabel) const
    {
        auto label = ReadOptionLabel(a_optionIndex);

        if (!label) {
            return false;
        }

        if (!a_optionLabel.empty()) {
            return *label == a_optionLabel;
        }

        return label->empty() && !a_rowLabel.label.empty() && ReadRowLabel(a_optionIndex) == a_rowLabel;
    }

    std::optional<MCMControl> MCMScript::ReadControl(int a_optionIndex) const
    {
        auto type = ReadControlType(a_optionIndex);
        auto label = ReadOptionLabel(a_optionIndex);
        if (!type || !label) {
            return std::nullopt;
        }

        // Some text buttons leave their label empty and put the visible command in the value buffer.
        if (label->empty() && *type == ControlType::Unknown) {
            label = ReadOptionText(a_optionIndex);
        }
        if (!label || (label->empty() && *type != ControlType::Cycle)) {
            return std::nullopt;
        }

        MCMControl control;
        control.optionLabel = std::move(*label);
        control.stateName = ReadStateName(a_optionIndex).value_or("");
        control.valueText = ReadOptionText(a_optionIndex).value_or("");
        control.type = *type;
        return control;
    }

    bool MCMScript::CanSelectOption(int a_optionIndex) const
    {
        auto flag = a_optionIndex >= 0 ? ReadNumber("_optionFlagsBuf", static_cast<size_t>(a_optionIndex)) : std::nullopt;
        if (!flag || *flag < 0) {
            return false;
        }
        const auto flagsOnly = GET_FLAGS_ONLY(static_cast<uint32_t>(*flag));
        return (flagsOnly & 2) == 0;
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
