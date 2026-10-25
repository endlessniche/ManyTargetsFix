#include "GlowGroups.h"
#include "GlowState.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstring>

namespace InstinctGlow {
    namespace {
        constexpr uint32_t c_IdBits = 0x3F;
        constexpr uint64_t c_LastCategoryKey = 0x1F;
        constexpr size_t c_GlowTypeOffset = 0x61;
        constexpr size_t c_IntensityOffset = 0x95;
        constexpr size_t c_UnconsciousOffset = 0x48;
        constexpr uint64_t c_UniqueLook = UINT64_MAX;

        uint8_t g_Cost[c_MaxLimit + 1];
        uint64_t g_Look[c_MaxLimit + 1];
        uint32_t g_Prefix[c_MaxLimit + 2];
        uint64_t g_GroupLooks[c_IdBits];
        uint32_t g_GroupHeads[c_IdBits];

        uint8_t GlowType(uint64_t p_Key) {
            if (p_Key < c_LastCategoryKey) {
                return static_cast<uint8_t>(p_Key);
            }

            return reinterpret_cast<const uint8_t*>(p_Key)[c_GlowTypeOffset];
        }

        uint8_t IdCost(uint8_t p_Type) {
            return p_Type <= c_LastTableType && g_TwoBitTypes[p_Type] ? 2 : 1;
        }

        uint64_t LookOf(const Entry& p_Entry, uint8_t p_Type) {
            if (p_Entry.Key <= c_LastCategoryKey) {
                return c_UniqueLook;
            }

            const auto* s_Node = reinterpret_cast<const uint8_t*>(p_Entry.Key);
            float s_Unconscious;
            memcpy(&s_Unconscious, s_Node + c_UnconsciousOffset, sizeof(s_Unconscious));
            const float s_Blend = s_Unconscious > 0.0f ? std::min(s_Unconscious, 1.0f) : 0.0f;

            const uint64_t s_IntensityLevel = s_Node[c_IntensityOffset] >> 4;
            const uint64_t s_BlendLevel = static_cast<uint64_t>(s_Blend * 4.0f + 0.5f);
            return p_Type | s_IntensityLevel << 8 | s_BlendLevel << 12;
        }

        bool FindGroup(uint64_t p_Look, uint32_t p_Groups, uint32_t& p_Group) {
            for (uint32_t i = 0; i < p_Groups; ++i) {
                if (g_GroupLooks[i] == p_Look) {
                    p_Group = i;
                    return true;
                }
            }

            return false;
        }

        bool Splice(Entry& p_Head, Entry& p_Member) {
            if (!p_Member.Links) {
                return true;
            }

            auto* s_Tail = reinterpret_cast<Link*>(p_Member.Links);

            for (uint32_t i = 0; s_Tail->Next && i < c_MaxLinks; ++i) {
                s_Tail = s_Tail->Next;
            }

            if (s_Tail->Next) {
                return false;
            }

            s_Tail->Next = reinterpret_cast<Link*>(p_Head.Links);
            p_Head.Links = p_Member.Links;
            p_Head.Linked = 1;
            p_Member.Links = 0;
            return true;
        }

        uint32_t GroupUnguarded(uint32_t p_Count) {
            Entry* s_Entries = g_List;
            uint32_t s_Total = 0;

            for (uint32_t i = 0; i < p_Count; ++i) {
                const uint8_t s_Type = GlowType(s_Entries[i].Key);
                g_Cost[i] = IdCost(s_Type);
                g_Look[i] = LookOf(s_Entries[i], s_Type);
                g_Prefix[i] = s_Total;
                s_Total += g_Cost[i];
            }

            g_Prefix[p_Count] = s_Total;

            if (s_Total <= c_IdBits) {
                return p_Count;
            }

            uint32_t s_Separate = p_Count;
            uint32_t s_Groups = 0;
            uint32_t s_RestCost = 0;

            while (g_Prefix[s_Separate] + s_RestCost > c_IdBits) {
                if (s_Separate == 0) {
                    return p_Count;
                }

                const uint32_t i = --s_Separate;
                uint32_t s_Group;

                if (g_Look[i] != c_UniqueLook && FindGroup(g_Look[i], s_Groups, s_Group)) {
                    continue;
                }

                if (g_Look[i] != c_UniqueLook) {
                    if (s_Groups == c_IdBits) {
                        return p_Count;
                    }

                    g_GroupLooks[s_Groups++] = g_Look[i];
                }

                s_RestCost += g_Cost[i];

                if (s_RestCost > c_IdBits) {
                    return p_Count;
                }
            }

            s_Groups = 0;
            uint32_t s_Out = s_Separate;

            for (uint32_t i = s_Separate; i < p_Count; ++i) {
                if (g_Look[i] != c_UniqueLook) {
                    uint32_t s_Group;

                    if (FindGroup(g_Look[i], s_Groups, s_Group)) {
                        Splice(s_Entries[g_GroupHeads[s_Group]], s_Entries[i]);
                        continue;
                    }

                    g_GroupLooks[s_Groups] = g_Look[i];
                    g_GroupHeads[s_Groups++] = s_Out;
                }

                if (s_Out != i) {
                    s_Entries[s_Out] = s_Entries[i];
                }

                ++s_Out;
            }

            return s_Out;
        }
    }

    uint32_t GroupEntries(uint32_t p_Count) {
        const uint32_t s_Count = std::min(p_Count, g_Limit);
        uint32_t s_Kept = p_Count;

        __try {
            s_Kept = GroupUnguarded(s_Count);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            s_Kept = p_Count;
        }

        g_LastGlowing = s_Count;
        return s_Kept;
    }
}
