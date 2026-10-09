#pragma once

#include "plugin.hpp"

#include <span>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// The public API uses ImGui declarations supplied by the FLICK demo's vcpkg overlay.
#include "menu/API/FUCK_API.h"

namespace MCMMemory::Menu
{
    class FLICKTool final : public FUCK::ITool
    {
    public:

        const char* Name() const override { return BEAUTIFUL_NAME; }

        void Draw() override;
    };
}
