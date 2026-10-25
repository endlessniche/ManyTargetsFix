#pragma once

#include "GameCode.h"

#include <cstdint>
#include <vector>

namespace InstinctGlow::Speedups {
    inline constexpr uint32_t c_Total = 3;

    uint32_t Prepare(const GameCode::Section& p_Node, uint8_t* p_Stub, std::vector<GameCode::Patch>& p_Patches);
    bool HookLookup();
    void* LookupTarget();

    void BeginNodeCall();
    void EndNodeCall();
}
