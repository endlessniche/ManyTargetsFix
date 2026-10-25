#pragma once

#include <cstdint>
#include <string>

namespace TrackerRays {
    std::string Install();
    void Uninstall();
    bool IsInstalled();

    void SetEnabled(bool p_Enabled);
    bool IsEnabled();

    uint32_t LastTargets();
    uint32_t LastSkipped();
}
