#pragma once

#include <cstdint>
#include <string>

namespace TouchBufferFix {
    std::string Install();
    void Uninstall();
    bool IsInstalled();

    void SetEnabled(bool p_Enabled);
    bool IsEnabled();

    uint32_t BatchesFixed();
}
