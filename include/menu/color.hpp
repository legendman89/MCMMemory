#pragma once

#include "menu/ui.hpp"

namespace MCMMemory::Menu::Color
{

    struct ButtonColors
    {
        GUI::ImVec4 hover;

        GUI::ImVec4 active;

        GUI::ImVec4 text;
    };

    struct CTAColors
    {
        GUI::ImVec4 background;

        GUI::ImVec4 hover;

        GUI::ImVec4 active;

        GUI::ImVec4 text;
    };

    inline constexpr CTAColors kBackupButtonColors
    {
        { 0.55F, 0.40F, 0.14F, 1.00F },
        { 0.70F, 0.53F, 0.21F, 1.00F },
        { 0.45F, 0.32F, 0.10F, 1.00F },
        { 1.00F, 0.95F, 0.82F, 1.00F }
    };

    inline constexpr CTAColors kRestoreButtonColors
    {
        { 0.26F, 0.43F, 0.23F, 1.00F },
        { 0.34F, 0.55F, 0.30F, 1.00F },
        { 0.20F, 0.34F, 0.18F, 1.00F },
        { 0.92F, 0.98F, 0.90F, 1.00F }
    };

    inline constexpr CTAColors kDisabledButtonColors
    {
        { 0.35F, 0.37F, 0.40F, 0.55F },
        { 0.40F, 0.42F, 0.46F, 0.60F },
        { 0.32F, 0.34F, 0.38F, 0.55F },
        { 0.60F, 0.62F, 0.68F, 1.00F }
    };

    inline constexpr ButtonColors kPreviewButtonColors
    {
        { 0.00F, 0.50F, 1.00F, 1.00F },
        { 0.00F, 0.40F, 0.80F, 1.00F },
        { 1.00F, 1.00F, 1.00F, 1.00F }
    };

    inline constexpr GUI::ImVec4 kIconHover{ 1.00F, 0.52F, 0.08F, 1.00F };
    inline constexpr GUI::ImVec4 kTransparent{ 0.0F, 0.0F, 0.0F, 0.0F };

}
