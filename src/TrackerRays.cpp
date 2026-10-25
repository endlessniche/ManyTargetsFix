#include "TrackerRays.h"
#include "GameCode.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

namespace TrackerRays {
    namespace {
        constexpr uint8_t c_TrackerUpdateSignature[] = {
            0x48, 0x89, 0x4C, 0x24, 0x08, 0x56, 0x41, 0x55, 0x48, 0x81, 0xEC, 0x98, 0x01, 0x00, 0x00, 0x48,
            0x8D, 0x71, 0x08, 0x4C, 0x8B, 0xE9, 0x48, 0x8B, 0x4E, 0x10,
        };

        constexpr uint8_t c_TargetCheckSignature[] = {
            0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20, 0x48,
            0x89, 0x4C, 0x24, 0x08, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C,
            0x24, 0xC9, 0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x4A, 0x38, 0x4C, 0x8D, 0x7A,
            0x28,
        };

        constexpr size_t c_EntryDistanceOffset = 0x1C;
        constexpr float c_NotVisiblePenalty = 20.0f;
        constexpr float c_SafetyMargin = 10.0f;
        constexpr float c_NoCutoff = std::numeric_limits<float>::infinity();

        using TrackerUpdateFn = void(*)(void*);
        using TargetCheckFn = void(*)(void*, uint8_t*);

        void* g_TrackerUpdateTarget = nullptr;
        void* g_TargetCheckTarget = nullptr;
        TrackerUpdateFn g_TrackerUpdateOriginal = nullptr;
        TargetCheckFn g_TargetCheckOriginal = nullptr;

        std::atomic<bool> g_Enabled = false;
        std::atomic<float> g_NextCutoff = c_NoCutoff;
        std::atomic<uint32_t> g_LastTargets = 0;
        std::atomic<uint32_t> g_LastSkipped = 0;
        std::atomic<int32_t> g_Calls = 0;

        thread_local bool t_InTrackerUpdate = false;
        thread_local bool t_Skip = false;
        thread_local float t_Cutoff = c_NoCutoff;
        thread_local float t_Nearest = c_NoCutoff;
        thread_local uint32_t t_Targets = 0;
        thread_local uint32_t t_Skipped = 0;

        void Detour_TrackerUpdate(void* p_Manager) {
            GameCode::CallGuard s_Guard(g_Calls);

            t_InTrackerUpdate = true;
            t_Skip = g_Enabled;
            t_Cutoff = g_NextCutoff;
            t_Nearest = c_NoCutoff;
            t_Targets = 0;
            t_Skipped = 0;

            g_TrackerUpdateOriginal(p_Manager);

            t_InTrackerUpdate = false;
            g_NextCutoff = t_Targets > 0 && std::isfinite(t_Nearest)
                ? t_Nearest + c_NotVisiblePenalty + c_SafetyMargin
                : c_NoCutoff;
            g_LastTargets = t_Targets;
            g_LastSkipped = t_Skipped;
        }

        void Detour_TargetCheck(void* p_Unused, uint8_t* p_Entry) {
            GameCode::CallGuard s_Guard(g_Calls);

            if (t_InTrackerUpdate) {
                const auto s_Distance = GameCode::Read<float>(p_Entry + c_EntryDistanceOffset);
                ++t_Targets;
                t_Nearest = (std::min)(t_Nearest, s_Distance);

                if (t_Skip && s_Distance > t_Cutoff) {
                    ++t_Skipped;
                    return;
                }
            }

            g_TargetCheckOriginal(p_Unused, p_Entry);
        }

        std::string HookBySignature(
            const GameCode::Section& p_Text, const uint8_t* p_Signature, size_t p_Size, void* p_Detour, void** p_Original,
            void*& p_Target, const char* p_Name
        ) {
            const uint8_t* s_Function = GameCode::FindUnique(p_Text, p_Signature, p_Size);

            if (!s_Function) {
                return GameCode::Mismatch(p_Name);
            }

            auto* s_Target = const_cast<uint8_t*>(s_Function);

            if (std::string s_Error = GameCode::Hook(s_Target, p_Detour, p_Original, p_Name); !s_Error.empty()) {
                return s_Error;
            }

            p_Target = s_Target;
            return {};
        }
    }

    std::string Install() {
        if (g_TrackerUpdateTarget) {
            return {};
        }

        GameCode::Section s_Text;

        if (!GameCode::GameText(s_Text)) {
            return "HITMAN3.exe has no .text section.";
        }

        std::string s_Error = HookBySignature(
            s_Text, c_TargetCheckSignature, sizeof(c_TargetCheckSignature), reinterpret_cast<void*>(&Detour_TargetCheck),
            reinterpret_cast<void**>(&g_TargetCheckOriginal), g_TargetCheckTarget, "The target visibility check"
        );

        if (s_Error.empty()) {
            s_Error = HookBySignature(
                s_Text, c_TrackerUpdateSignature, sizeof(c_TrackerUpdateSignature),
                reinterpret_cast<void*>(&Detour_TrackerUpdate), reinterpret_cast<void**>(&g_TrackerUpdateOriginal),
                g_TrackerUpdateTarget, "The target tracker"
            );
        }

        if (!s_Error.empty()) {
            Uninstall();
        }

        return s_Error;
    }

    void Uninstall() {
        GameCode::Unhook({g_TrackerUpdateTarget, g_TargetCheckTarget}, g_Calls);
        g_TrackerUpdateTarget = nullptr;
        g_TargetCheckTarget = nullptr;
    }

    bool IsInstalled() {
        return g_TrackerUpdateTarget != nullptr;
    }

    void SetEnabled(bool p_Enabled) {
        g_Enabled = p_Enabled;
    }

    bool IsEnabled() {
        return g_Enabled;
    }

    uint32_t LastTargets() {
        return g_LastTargets;
    }

    uint32_t LastSkipped() {
        return g_LastSkipped;
    }
}
