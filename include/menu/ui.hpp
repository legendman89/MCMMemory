#pragma once

#include "plugin.hpp"
#include "menu/backend.hpp"
#include "menu/API/SKSEMenuFramework.h"

namespace MCMMemory::GUI::SMF
{
    using namespace ImGuiMCP;

    inline constexpr PushIDStringFunction PushIDString = ImGuiMCP::PushID;
    inline constexpr PushIDIntFunction PushIDInt = ImGuiMCP::PushID;

    inline bool Button(const char* a_label, float a_width, float a_height)
    {
        return ImGuiMCP::Button(a_label, ImGuiMCP::ImVec2{ a_width, a_height });
    }

    inline bool InputText(const char* a_label, char* a_buffer, size_t a_size)
    {
        return ImGuiMCP::InputText(a_label, a_buffer, a_size);
    }

    inline bool SliderFloat(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format)
    {
        return ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);
    }

    inline bool SliderInt(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format)
    {
        return ImGuiMCP::SliderInt(a_label, a_value, a_min, a_max, a_format);
    }
}

namespace MCMMemory::GUI
{
    using namespace ImGuiMCP;

    inline bool Button(const char* a_label, const ImGuiMCP::ImVec2 a_size)
    {
        return GUI::Button(a_label, a_size.x, a_size.y);
    }
}
