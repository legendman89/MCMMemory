#pragma once

#include "plugin.hpp"
#include "menu/backend.hpp"
#include "menu/API/SKSEMenuFramework.h"

namespace MCMMemory::GUI
{
    using namespace ImGuiMCP;

    inline bool Button(const char* a_label, const ImGuiMCP::ImVec2 a_size)
    {
        return GUI::Button(a_label, a_size.x, a_size.y);
    }
}
