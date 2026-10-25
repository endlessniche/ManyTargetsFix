#pragma once

#include <atomic>
#include <cstdint>

namespace InstinctGlow {
    inline constexpr uint32_t c_GameLimit = 64;
    inline constexpr uint32_t c_MaxLimit = 1024;
    inline constexpr uint32_t c_MaxLinks = 16384;
    inline constexpr uint8_t c_LastTableType = 0x1E;

    struct Entry {
        float Value;
        int32_t Priority;
        uint32_t Linked;
        uint32_t Unknown;
        uint64_t Key;
        uint64_t Links;
    };

    static_assert(sizeof(Entry) == 0x20);

    struct Link {
        uint64_t Primitive;
        Link* Next;
    };

    inline Entry* g_List = nullptr;
    inline uint32_t g_Limit = c_GameLimit;
    inline bool g_TwoBitTypes[c_LastTableType + 1] {};
    inline std::atomic<bool> g_Active = false;
    inline std::atomic<int32_t> g_Calls = 0;
    inline std::atomic<uint32_t> g_NodeThread = 0;
    inline uint32_t g_NodeCall = 1;
    inline std::atomic<uint32_t> g_LastGlowing = 0;
}
