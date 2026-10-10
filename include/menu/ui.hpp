#pragma once

#include "plugin.hpp"
#include "menu/backend.hpp"
#include "menu/API/SKSEMenuFramework.h"

namespace MCMMemory::GUI
{
    using namespace ImGuiMCP;

    inline bool BeginTable(const char* a_id, int a_columns, int a_flags, const ImGuiMCP::ImVec2 a_size, float a_innerWidth = 0.0F)
    {
        return GUI::BeginTable(a_id, a_columns, a_flags, a_size.x, a_size.y, a_innerWidth);
    }

    inline bool Button(const char* a_label, const ImGuiMCP::ImVec2 a_size)
    {
        return GUI::Button(a_label, a_size.x, a_size.y);
    }
}
