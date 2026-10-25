#include "TouchBufferFix.h"
#include "GameCode.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace TouchBufferFix {
    namespace {
        constexpr size_t c_SlotExecute = 0;
        constexpr size_t c_SlotSetUserMemory = 6;
        constexpr size_t c_SlotGetUserMemory = 7;
        constexpr size_t c_SlotRelease = 8;
        constexpr size_t c_SlotRaycast = 9;
        constexpr size_t c_SlotOverlap = 10;
        constexpr size_t c_SlotSweep = 11;
        constexpr size_t c_SlotCount = 13;

        constexpr std::pair<size_t, const char*> c_Anchors[] = {
            {c_SlotExecute, "PxBatchQuery::execute: This batch is already executing"},
            {c_SlotSetUserMemory, "PxBatchQuery::setUserMemory: This batch is still executing, skipping setUserMemory"},
            {c_SlotRelease, "PxBatchQuery::release: This batch is still executing, skipping release"},
            {c_SlotRaycast, "PxBatchQuery::raycast: This batch is still executing, skipping query."},
            {c_SlotOverlap, "PxBatchQuery::overlap: This batch is still executing, skipping query."},
            {c_SlotSweep, "PxBatchQuery::sweep: This batch is still executing, skipping query."},
        };

        constexpr size_t c_QueryMemorySize = 72;
        constexpr size_t c_RaycastTouchBufferOffset = 8;
        constexpr size_t c_RaycastTouchCapacityOffset = 48;
        constexpr size_t c_RaycastHitSize = 64;
        constexpr uint32_t c_MaxTouchCapacity = 65536;

        using ExecuteFn = void(*)(void*);
        using GetUserMemoryFn = const uint8_t*(*)(void*);
        using SetUserMemoryFn = void(*)(void*, const uint8_t*);

        struct TouchBuffer {
            uintptr_t GameBuffer = 0;
            std::unique_ptr<uint8_t[]> Ours;
            bool Installed = false;
        };

        std::mutex g_Mutex;
        void** g_VTable = nullptr;
        void* g_OriginalExecute = nullptr;
        void* g_Original[c_SlotCount] {};
        std::atomic<bool> g_Enabled = false;
        std::atomic<int32_t> g_Calls = 0;
        std::unordered_map<uintptr_t, TouchBuffer> g_Buffers;

        const uint8_t* FindString(const GameCode::Section& p_Section, const char* p_String) {
            const auto* s_Needle = reinterpret_cast<const uint8_t*>(p_String);
            const auto* s_Found = std::search(p_Section.Begin, p_Section.End(), s_Needle, s_Needle + strlen(p_String) + 1);
            return s_Found == p_Section.End() ? nullptr : s_Found;
        }

        std::unordered_set<const void*> FunctionsReferencing(const GameCode::Section& p_Text, const uint8_t* p_Target) {
            std::unordered_set<const void*> s_Functions;
            const auto s_Target = reinterpret_cast<uintptr_t>(p_Target);

            for (size_t i = 0; i + 7 <= p_Text.Size; ++i) {
                const uint8_t* s_Instruction = p_Text.Begin + i;

                if ((s_Instruction[0] != 0x48 && s_Instruction[0] != 0x4C) || s_Instruction[1] != 0x8D ||
                    (s_Instruction[2] & 0xC7) != 0x05) {
                    continue;
                }

                if (reinterpret_cast<uintptr_t>(GameCode::RipTarget(s_Instruction, 3, 7)) == s_Target) {
                    if (const auto* s_Start = GameCode::FunctionStart(s_Instruction)) {
                        s_Functions.insert(s_Start);
                    }
                }
            }

            return s_Functions;
        }

        void** FindVTable(HMODULE p_PhysX, std::string& p_Error) {
            GameCode::Section s_Text, s_RData;

            if (!GameCode::FindSection(p_PhysX, ".text", s_Text) || !GameCode::FindSection(p_PhysX, ".rdata", s_RData)) {
                p_Error = "PhysX3_x64.dll has no .text or .rdata section.";
                return nullptr;
            }

            std::unordered_set<const void*> s_Candidates[c_SlotCount];

            for (const auto& [s_Slot, s_String] : c_Anchors) {
                const uint8_t* s_Found = FindString(s_RData, s_String);
                s_Candidates[s_Slot] = s_Found ? FunctionsReferencing(s_Text, s_Found) : std::unordered_set<const void*>();

                if (s_Candidates[s_Slot].empty()) {
                    p_Error = "This version of PhysX3_x64.dll isn't supported.";
                    return nullptr;
                }
            }

            auto** s_Slots = reinterpret_cast<void**>(const_cast<uint8_t*>(s_RData.Begin));
            const size_t s_SlotTotal = s_RData.Size / sizeof(void*);

            for (size_t i = 0; i + c_SlotCount <= s_SlotTotal; ++i) {
                void** s_VTable = s_Slots + i;

                if (std::ranges::all_of(c_Anchors, [&](const auto& p_Anchor) {
                    return s_Candidates[p_Anchor.first].contains(s_VTable[p_Anchor.first]);
                })) {
                    return s_VTable;
                }
            }

            p_Error = "PhysX's batch query functions were not found. Another mod may already be patching them.";
            return nullptr;
        }

        bool WriteSlot(void** p_VTable, size_t p_Slot, void* p_Value) {
            DWORD s_OldProtect;

            if (!VirtualProtect(&p_VTable[p_Slot], sizeof(void*), PAGE_READWRITE, &s_OldProtect)) {
                return false;
            }

            InterlockedExchangePointer(&p_VTable[p_Slot], p_Value);
            VirtualProtect(&p_VTable[p_Slot], sizeof(void*), s_OldProtect, &s_OldProtect);
            return true;
        }

        void ApplyFix(void* p_Batch) {
            const uint8_t* s_Memory = reinterpret_cast<GetUserMemoryFn>(g_Original[c_SlotGetUserMemory])(p_Batch);
            uint8_t s_Copy[c_QueryMemorySize];
            memcpy(s_Copy, s_Memory, sizeof(s_Copy));

            const auto s_Current = GameCode::Read<uintptr_t>(s_Copy + c_RaycastTouchBufferOffset);
            const auto s_Capacity = GameCode::Read<uint32_t>(s_Copy + c_RaycastTouchCapacityOffset);

            if (s_Current == 0 || s_Capacity == 0 || s_Capacity > c_MaxTouchCapacity) {
                return;
            }

            const bool s_Enabled = g_Enabled;
            uintptr_t s_Wanted;

            {
                std::scoped_lock s_Lock(g_Mutex);
                TouchBuffer& s_Buffer = g_Buffers[reinterpret_cast<uintptr_t>(p_Batch)];

                if (!s_Buffer.Ours) {
                    s_Buffer.GameBuffer = s_Current;
                    s_Buffer.Ours = std::make_unique<uint8_t[]>(static_cast<size_t>(s_Capacity) * c_RaycastHitSize);
                }

                s_Wanted = s_Enabled ? reinterpret_cast<uintptr_t>(s_Buffer.Ours.get()) : s_Buffer.GameBuffer;
                s_Buffer.Installed = s_Enabled;
            }

            if (s_Current != s_Wanted) {
                memcpy(s_Copy + c_RaycastTouchBufferOffset, &s_Wanted, sizeof(s_Wanted));
                reinterpret_cast<SetUserMemoryFn>(g_Original[c_SlotSetUserMemory])(p_Batch, s_Copy);
            }
        }

        void Detour_Execute(void* p_Batch) {
            GameCode::CallGuard s_Guard(g_Calls);
            ApplyFix(p_Batch);
            reinterpret_cast<ExecuteFn>(g_OriginalExecute)(p_Batch);
        }
    }

    std::string Install() {
        std::scoped_lock s_Lock(g_Mutex);

        if (g_VTable) {
            return {};
        }

        const HMODULE s_PhysX = GetModuleHandleW(L"PhysX3_x64.dll");

        if (!s_PhysX) {
            return "PhysX3_x64.dll is not loaded.";
        }

        std::string s_Error;
        void** s_VTable = FindVTable(s_PhysX, s_Error);

        if (!s_VTable) {
            return s_Error;
        }

        std::copy_n(s_VTable, c_SlotCount, g_Original);
        g_OriginalExecute = s_VTable[c_SlotExecute];

        if (!WriteSlot(s_VTable, c_SlotExecute, reinterpret_cast<void*>(&Detour_Execute))) {
            return std::format("Could not patch PhysX's batch execute (error {}).", GetLastError());
        }

        g_VTable = s_VTable;
        return {};
    }

    void Uninstall() {
        bool s_Restored = false;

        {
            std::scoped_lock s_Lock(g_Mutex);

            if (g_VTable && WriteSlot(g_VTable, c_SlotExecute, g_OriginalExecute)) {
                g_VTable = nullptr;
                s_Restored = true;
            }
        }

        if (s_Restored) {
            GameCode::WaitForCalls(g_Calls);
        }

        std::scoped_lock s_Lock(g_Mutex);

        for (auto& [s_Batch, s_Buffer] : g_Buffers) {
            static_cast<void>(s_Buffer.Ours.release());
        }
    }

    bool IsInstalled() {
        std::scoped_lock s_Lock(g_Mutex);
        return g_VTable != nullptr;
    }

    void SetEnabled(bool p_Enabled) {
        g_Enabled = p_Enabled;
    }

    bool IsEnabled() {
        return g_Enabled;
    }

    uint32_t BatchesFixed() {
        std::scoped_lock s_Lock(g_Mutex);
        uint32_t s_Count = 0;

        for (const auto& [s_Batch, s_Buffer] : g_Buffers) {
            s_Count += s_Buffer.Installed;
        }

        return s_Count;
    }
}
