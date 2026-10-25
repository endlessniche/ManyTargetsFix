#pragma once

#include <cstdint>
#include <string>

namespace InstinctGlow {
    std::string Install();
    void Uninstall();
    bool IsInstalled();

    void SetEnabled(bool p_Enabled);
    bool IsEnabled();

    uint32_t LastGlowing();
    uint32_t SpeedupsFound();
    uint32_t SpeedupsTotal();
}
