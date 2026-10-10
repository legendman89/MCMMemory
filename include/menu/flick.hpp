#pragma once

#include "plugin.hpp"
#include "menu/backend.hpp"
#include "menu/API/FUCK_API.h"

namespace MCMMemory::GUI::FLICK
{
    using namespace FUCK;

    inline constexpr PushIDStringFunction PushIDString = FUCK::PushID;
    inline constexpr PushIDIntFunction PushIDInt = FUCK::PushID;

    inline bool Checkbox(const char* a_label, bool* a_value)
    {
        return FUCK::Checkbox(a_label, a_value, false, false);
    }

    inline bool Button(const char* a_label, float, float)
    {
        return FUCK::Button(a_label);
    }

    inline void Spacing()
    {
        FUCK::Spacing();
    }

    inline void HelpMarker(const char* a_text)
    {
        FUCK::SameLine(0.0F, 6.0F);
        FUCK::HelpMarker(a_text);
    }

    inline float GetCursorPosX()
    {
        return FUCK::GetCursorPos().x;
    }

    inline bool InputText(const char* a_label, char* a_buffer, size_t a_size)
    {
        return FUCK::InputText(a_label, a_buffer, a_size);
    }
}

namespace MCMMemory::Menu
{
    class FLICKTool final : public FUCK::ITool
    {
    public:

        const char* Name() const override { return BEAUTIFUL_NAME; }

        void Draw() override;
    };
}
