#include "InstinctGlow.h"
#include "GameCode.h"
#include "GlowGroups.h"
#include "GlowSpeedups.h"
#include "GlowState.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <vector>

namespace InstinctGlow {
    namespace {
        constexpr uint8_t c_NodeSignature[] = {
            0x44, 0x88, 0x4C, 0x24, 0x20, 0x89, 0x54, 0x24, 0x10, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x53, 0x56,
            0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x50, 0x0C, 0x00, 0x00, 0x48, 0x8D,
            0xAC, 0x24, 0x80, 0x00, 0x00, 0x00,
        };

        constexpr uint8_t c_StoreBlock[] = {
            0xF3, 0x0F, 0x11, 0x94, 0x05, 0x30, 0x03, 0x00, 0x00, 0x44, 0x89, 0x84, 0x05, 0x34, 0x03, 0x00,
            0x00, 0x48, 0x89, 0xBC, 0x05, 0x48, 0x03, 0x00, 0x00, 0x4C, 0x89, 0xBC, 0x05, 0x40, 0x03, 0x00,
            0x00, 0x89, 0xBC, 0x05, 0x38, 0x03, 0x00, 0x00,
        };

        constexpr uint8_t c_RelocatedStores[] = {
            0xF3, 0x0F, 0x11, 0x10, 0x44, 0x89, 0x40, 0x04, 0x48, 0x89, 0x78, 0x18, 0x4C, 0x89, 0x78, 0x10,
            0x89, 0x78, 0x08,
        };

        constexpr uint8_t c_LimitInstruction[] = {0xBE, 0x40, 0x00, 0x00, 0x00};

        constexpr uint8_t c_PoolBlock[] = {
            0x48, 0x8D, 0x8C, 0x24, 0x80, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC1, 0x07, 0x48, 0x83, 0xE1, 0xF8,
            0x48, 0x89, 0x8D, 0x90, 0x00, 0x00, 0x00,
        };

        constexpr size_t c_PoolBlockPatched = 16;
        constexpr uint8_t c_PoolCapacity[] = {0xC7, 0x85, 0x98, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
        constexpr size_t c_PoolCapacityImmediate = 6;
        constexpr size_t c_LinkSize = 0x10;

        constexpr int16_t c_BudgetLoop[] = {
            0x8B, 0xC7, 0x8B, 0xDF, 0x89, 0x85, 0x40, 0x0C, 0x00, 0x00, 0x41, 0xB8, 0x3F, 0x00, 0x00, 0x00,
            0x89, 0x9D, 0x44, 0x0C, 0x00, 0x00, 0x85, 0xF6, 0x74, 0x76, 0x48, 0x8D, 0x95, 0x40, 0x03, 0x00,
            0x00, 0x44, 0x8B, 0xCE, 0x48, 0x8D, 0x3D, -1, -1, -1, -1, 0x48, 0x8B, 0x02, 0x48, 0x83,
            0xF8, 0x1F, 0x72, 0x04, 0x0F, 0xB6, 0x40, 0x61, 0x0F, 0xB6, 0xC8, 0x83, 0xF9, 0x1E, 0x77, 0x22,
            0x0F, 0xB6, 0x84, 0x0F, -1, -1, -1, -1, 0x8B, 0x8C, 0x87, -1, -1, -1, -1, 0x48,
            0x03, 0xCF, 0xFF, 0xE1, 0xB8, 0x02, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x8D, 0x44, 0x0C, 0x00, 0x00,
            0xEB, 0x0C, 0x48, 0x8D, 0x8D, 0x40, 0x0C, 0x00, 0x00, 0xB8, 0x01, 0x00, 0x00, 0x00,
        };

        constexpr size_t c_BudgetBlockPatched = 16;
        constexpr size_t c_BudgetImageBaseLoad = 36;
        constexpr size_t c_BudgetIndexTable = 68;
        constexpr size_t c_BudgetJumpTable = 75;
        constexpr size_t c_BudgetTwoBitBlock = 84;
        constexpr size_t c_BudgetOneBitBlock = 98;

        constexpr uint8_t c_IntensityRead[] = {
            0x49, 0x83, 0xFF, 0x1F, 0x76, 0x16, 0x41, 0x0F, 0xB6, 0x87, 0x95, 0x00, 0x00, 0x00, 0x66, 0x0F,
            0x6E, 0xD8, 0x0F, 0x5B, 0xDB, 0xF3, 0x41, 0x0F, 0x59, 0xDA, 0xEB, 0x03,
        };

        constexpr uint8_t c_BlendRead[] = {
            0x49, 0x83, 0xFF, 0x1F, 0x76, 0x08, 0xF3, 0x41, 0x0F, 0x10, 0x4F, 0x48, 0xEB, 0x04, 0x41, 0x0F,
            0x28, 0xCF,
        };

        constexpr uint8_t c_StubSave[] = {
            0x50, 0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x48, 0x81, 0xEC, 0x88, 0x00,
            0x00, 0x00, 0x0F, 0x29, 0x44, 0x24, 0x20, 0x0F, 0x29, 0x4C, 0x24, 0x30, 0x0F, 0x29, 0x54, 0x24,
            0x40, 0x0F, 0x29, 0x5C, 0x24, 0x50, 0x0F, 0x29, 0x64, 0x24, 0x60, 0x0F, 0x29, 0x6C, 0x24, 0x70,
            0x8B, 0xCE, 0x48, 0xB8,
        };

        constexpr uint8_t c_StubRestore[] = {
            0xFF, 0xD0, 0x8B, 0xF0, 0x0F, 0x28, 0x44, 0x24, 0x20, 0x0F, 0x28, 0x4C, 0x24, 0x30, 0x0F, 0x28,
            0x54, 0x24, 0x40, 0x0F, 0x28, 0x5C, 0x24, 0x50, 0x0F, 0x28, 0x64, 0x24, 0x60, 0x0F, 0x28, 0x6C,
            0x24, 0x70, 0x48, 0x81, 0xC4, 0x88, 0x00, 0x00, 0x00, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41,
            0x58, 0x5A, 0x59, 0x58,
        };

        constexpr int32_t c_ListBase = 0x330;
        constexpr int32_t c_ListFields[] = {0x330, 0x338, 0x340, 0x348};
        constexpr size_t c_ListLeaCount = 11;
        constexpr size_t c_LeaSize = 7;
        constexpr size_t c_PoolPointerOffset = 8;
        constexpr size_t c_ListOffsetInRegion = 0x40;
        constexpr size_t c_RegionSize = c_ListOffsetInRegion + (c_MaxLimit + 1) * sizeof(Entry);
        constexpr size_t c_StubSize = 0x1000;

        using NodeFn = void(*)(
            uint32_t, uint32_t, float, uint8_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
            uint64_t
        );

        uint8_t* g_Node = nullptr;
        NodeFn g_NodeOriginal = nullptr;
        uint8_t* g_Region = nullptr;
        uint8_t* g_Pool = nullptr;
        uint8_t* g_Stub = nullptr;
        uint8_t* g_LimitImmediate = nullptr;
        std::vector<GameCode::Patch> g_Patches;
        uint32_t g_SpeedupsFound = 0;

        SRWLOCK g_Lock = SRWLOCK_INIT;
        bool g_Patched = false;

        void FreeMemory() {
            if (g_Stub) {
                VirtualFree(g_Stub, 0, MEM_RELEASE);
                g_Stub = nullptr;
            }

            if (g_Pool) {
                VirtualFree(g_Pool, 0, MEM_RELEASE);
                g_Pool = nullptr;
            }

            if (g_Region) {
                VirtualFree(g_Region, 0, MEM_RELEASE);
                g_Region = nullptr;
                g_List = nullptr;
            }
        }

        void Detour_Node(
            uint32_t p_1, uint32_t p_2, float p_3, uint8_t p_4, uint64_t p_5, uint64_t p_6, uint64_t p_7, uint64_t p_8,
            uint64_t p_9, uint64_t p_10, uint64_t p_11, uint64_t p_12
        ) {
            GameCode::CallGuard s_Guard(g_Calls);
            AcquireSRWLockShared(&g_Lock);

            Speedups::BeginNodeCall();
            g_LastGlowing = 0;
            g_NodeThread = GetCurrentThreadId();
            g_NodeOriginal(p_1, p_2, p_3, p_4, p_5, p_6, p_7, p_8, p_9, p_10, p_11, p_12);
            g_NodeThread = 0;
            Speedups::EndNodeCall();

            ReleaseSRWLockShared(&g_Lock);
        }

        void WriteLimit(uint32_t p_Limit) {
            GameCode::WriteCode(g_LimitImmediate, reinterpret_cast<const uint8_t*>(&p_Limit), sizeof(p_Limit));
        }

        bool ReadTwoBitTypes(const uint8_t* p_Budget) {
            const uint8_t* s_ImageBase = GameCode::RipTarget(p_Budget + c_BudgetImageBaseLoad, 3, 7);

            if (s_ImageBase != reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr))) {
                return false;
            }

            const uint8_t* s_IndexTable = s_ImageBase + GameCode::Read<int32_t>(p_Budget + c_BudgetIndexTable);
            const uint8_t* s_JumpTable = s_ImageBase + GameCode::Read<int32_t>(p_Budget + c_BudgetJumpTable);

            for (uint8_t s_Type = 0; s_Type <= c_LastTableType; ++s_Type) {
                const auto s_Rva = GameCode::Read<uint32_t>(s_JumpTable + s_IndexTable[s_Type] * sizeof(uint32_t));
                const uint8_t* s_Target = s_ImageBase + s_Rva;

                if (s_Target == p_Budget + c_BudgetTwoBitBlock) {
                    g_TwoBitTypes[s_Type] = true;
                }
                else if (s_Target == p_Budget + c_BudgetOneBitBlock) {
                    g_TwoBitTypes[s_Type] = false;
                }
                else {
                    return false;
                }
            }

            return true;
        }

        std::string PrepareListLeas(const GameCode::Section& p_Node) {
            auto* s_List = reinterpret_cast<uint8_t*>(g_List);
            size_t s_Found = 0;

            for (const uint8_t* p = p_Node.Begin; p + c_LeaSize <= p_Node.End(); ++p) {
                if ((p[0] != 0x48 && p[0] != 0x4C) || p[1] != 0x8D || (p[2] & 0xC7) != 0x85) {
                    continue;
                }

                const auto s_Field = GameCode::Read<int32_t>(p + 3);

                if (std::ranges::find(c_ListFields, s_Field) == std::end(c_ListFields)) {
                    continue;
                }

                int32_t s_Displacement;

                if (!GameCode::Displacement(p + c_LeaSize, s_List + (s_Field - c_ListBase), s_Displacement)) {
                    return "The glow list could not be placed close enough to the game's code.";
                }

                GameCode::Patch s_Patch;
                s_Patch.Address = const_cast<uint8_t*>(p);
                s_Patch.Original.assign(p, p + c_LeaSize);
                s_Patch.Patched = {p[0], 0x8D, static_cast<uint8_t>(p[2] & 0x3F)};
                s_Patch.Patched.resize(c_LeaSize);
                memcpy(s_Patch.Patched.data() + 3, &s_Displacement, sizeof(s_Displacement));
                g_Patches.push_back(std::move(s_Patch));
                ++s_Found;
            }

            if (s_Found != c_ListLeaCount) {
                return GameCode::Mismatch(std::format("The glow list's code ({} of {} references found)", s_Found, c_ListLeaCount));
            }

            return {};
        }

        std::string PrepareListStores(const GameCode::Section& p_Node) {
            const uint8_t* s_Stores = GameCode::FindUnique(p_Node, c_StoreBlock, sizeof(c_StoreBlock));
            const uint8_t* s_Limit = GameCode::FindUnique(p_Node, c_LimitInstruction, sizeof(c_LimitInstruction));

            if (!s_Stores || !s_Limit) {
                return GameCode::Mismatch("The glow list's code");
            }

            int32_t s_Displacement;

            if (!GameCode::Displacement(s_Stores + 7, g_Region, s_Displacement)) {
                return "The glow list could not be placed close enough to the game's code.";
            }

            GameCode::Patch s_Patch;
            s_Patch.Address = const_cast<uint8_t*>(s_Stores);
            s_Patch.Original.assign(s_Stores, s_Stores + sizeof(c_StoreBlock));
            s_Patch.Patched = {0x48, 0x03, 0x05, 0, 0, 0, 0};
            memcpy(s_Patch.Patched.data() + 3, &s_Displacement, sizeof(s_Displacement));
            s_Patch.Patched.insert(s_Patch.Patched.end(), std::begin(c_RelocatedStores), std::end(c_RelocatedStores));
            const size_t s_Padding = sizeof(c_StoreBlock) - s_Patch.Patched.size();
            s_Patch.Patched.push_back(0xEB);
            s_Patch.Patched.push_back(static_cast<uint8_t>(s_Padding - 2));
            s_Patch.Patched.resize(sizeof(c_StoreBlock), 0x90);
            g_Patches.push_back(std::move(s_Patch));

            g_LimitImmediate = const_cast<uint8_t*>(s_Limit) + 1;
            return {};
        }

        std::string PreparePool(const GameCode::Section& p_Node) {
            const uint8_t* s_PoolBlock = GameCode::FindUnique(p_Node, c_PoolBlock, sizeof(c_PoolBlock));
            const uint8_t* s_PoolCapacity = GameCode::FindUnique(p_Node, c_PoolCapacity, sizeof(c_PoolCapacity));

            if (!s_PoolBlock || !s_PoolCapacity) {
                return GameCode::Mismatch("The glow pool's code");
            }

            int32_t s_Displacement;

            if (!GameCode::Displacement(s_PoolBlock + 7, g_Region + c_PoolPointerOffset, s_Displacement)) {
                return "The glow pool could not be placed close enough to the game's code.";
            }

            GameCode::Patch s_Pool;
            s_Pool.Address = const_cast<uint8_t*>(s_PoolBlock);
            s_Pool.Original.assign(s_PoolBlock, s_PoolBlock + c_PoolBlockPatched);
            s_Pool.Patched = {0x48, 0x8B, 0x0D, 0, 0, 0, 0, 0xEB, 0x07};
            memcpy(s_Pool.Patched.data() + 3, &s_Displacement, sizeof(s_Displacement));
            s_Pool.Patched.resize(c_PoolBlockPatched, 0x90);
            g_Patches.push_back(std::move(s_Pool));

            GameCode::Patch s_Capacity;
            s_Capacity.Address = const_cast<uint8_t*>(s_PoolCapacity);
            s_Capacity.Original.assign(s_PoolCapacity, s_PoolCapacity + sizeof(c_PoolCapacity));
            s_Capacity.Patched = s_Capacity.Original;
            memcpy(s_Capacity.Patched.data() + c_PoolCapacityImmediate, &c_MaxLinks, sizeof(c_MaxLinks));
            g_Patches.push_back(std::move(s_Capacity));
            return {};
        }

        std::string PrepareBudget(const GameCode::Section& p_Node) {
            const uint8_t* s_Found = GameCode::FindUnique(p_Node, c_BudgetLoop, std::size(c_BudgetLoop));

            if (!s_Found || !ReadTwoBitTypes(s_Found)) {
                return GameCode::Mismatch("The glow ID budget's code");
            }

            if (!GameCode::FindUnique(p_Node, c_IntensityRead, sizeof(c_IntensityRead)) ||
                !GameCode::FindUnique(p_Node, c_BlendRead, sizeof(c_BlendRead))) {
                return GameCode::Mismatch("The glow fade code");
            }

            auto* s_Budget = const_cast<uint8_t*>(s_Found);
            const uint64_t s_Group = reinterpret_cast<uint64_t>(&GroupEntries);
            const auto* s_GroupBytes = reinterpret_cast<const uint8_t*>(&s_Group);

            std::vector<uint8_t> s_Code(std::begin(c_StubSave), std::end(c_StubSave));
            s_Code.insert(s_Code.end(), s_GroupBytes, s_GroupBytes + sizeof(s_Group));
            s_Code.insert(s_Code.end(), std::begin(c_StubRestore), std::end(c_StubRestore));
            s_Code.insert(s_Code.end(), s_Budget, s_Budget + c_BudgetBlockPatched);
            s_Code.push_back(0xE9);

            int32_t s_Back;
            int32_t s_Into;

            if (!GameCode::Displacement(g_Stub + s_Code.size() + sizeof(s_Back), s_Budget + c_BudgetBlockPatched, s_Back) ||
                !GameCode::Displacement(s_Budget + 5, g_Stub, s_Into)) {
                return "The glow ID budget could not be reached from memory close to the game's code.";
            }

            s_Code.resize(s_Code.size() + sizeof(s_Back));
            memcpy(s_Code.data() + s_Code.size() - sizeof(s_Back), &s_Back, sizeof(s_Back));
            memcpy(g_Stub, s_Code.data(), s_Code.size());

            GameCode::Patch s_Patch;
            s_Patch.Address = s_Budget;
            s_Patch.Original.assign(s_Budget, s_Budget + c_BudgetBlockPatched);
            s_Patch.Patched = {0xE9, 0, 0, 0, 0};
            memcpy(s_Patch.Patched.data() + 1, &s_Into, sizeof(s_Into));
            s_Patch.Patched.resize(c_BudgetBlockPatched, 0x90);
            g_Patches.push_back(std::move(s_Patch));
            return {};
        }

        std::string PreparePatches(const GameCode::Section& p_Node) {
            g_Patches.clear();

            for (const auto s_Step : {PrepareListLeas, PrepareListStores, PreparePool, PrepareBudget}) {
                if (std::string s_Error = s_Step(p_Node); !s_Error.empty()) {
                    return s_Error;
                }
            }

            g_SpeedupsFound = Speedups::Prepare(p_Node, g_Stub, g_Patches);

            DWORD s_OldProtect;

            if (!VirtualProtect(g_Stub, c_StubSize, PAGE_EXECUTE_READ, &s_OldProtect)) {
                return "The glow ID budget's code could not be made executable.";
            }

            FlushInstructionCache(GetCurrentProcess(), g_Stub, c_StubSize);
            return {};
        }

        std::string AllocateMemory(const uint8_t* p_Node) {
            g_Region = GameCode::AllocateNear(p_Node, c_RegionSize);
            g_Stub = GameCode::AllocateNear(p_Node, c_StubSize);
            g_Pool = static_cast<uint8_t*>(VirtualAlloc(nullptr, c_MaxLinks * c_LinkSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

            if (!g_Region || !g_Stub || !g_Pool) {
                FreeMemory();
                return "No memory was free close enough to the game's code for the glow list.";
            }

            g_List = reinterpret_cast<Entry*>(g_Region + c_ListOffsetInRegion);
            *reinterpret_cast<Entry**>(g_Region) = g_List;
            *reinterpret_cast<uint8_t**>(g_Region + c_PoolPointerOffset) = g_Pool;
            return {};
        }
    }

    std::string Install() {
        if (g_Node) {
            return {};
        }

        GameCode::Section s_Text;

        if (!GameCode::GameText(s_Text)) {
            return "HITMAN3.exe has no .text section.";
        }

        const uint8_t* s_Found = GameCode::FindUnique(s_Text, c_NodeSignature, sizeof(c_NodeSignature));
        GameCode::Section s_Node;

        if (!s_Found || !GameCode::FunctionBody(s_Found, s_Node) || s_Node.Begin != s_Found) {
            return GameCode::Mismatch("The Instinct glow code");
        }

        if (std::string s_Error = AllocateMemory(s_Found); !s_Error.empty()) {
            return s_Error;
        }

        if (std::string s_Error = PreparePatches(s_Node); !s_Error.empty()) {
            FreeMemory();
            return s_Error;
        }

        auto* s_Target = const_cast<uint8_t*>(s_Found);

        if (std::string s_Error = GameCode::Hook(
            s_Target, reinterpret_cast<void*>(&Detour_Node), reinterpret_cast<void**>(&g_NodeOriginal),
            "The Instinct glow code"
        ); !s_Error.empty()) {
            FreeMemory();
            return s_Error;
        }

        g_Node = s_Target;
        g_SpeedupsFound += Speedups::HookLookup();
        return {};
    }

    void Uninstall() {
        if (!g_Node) {
            return;
        }

        AcquireSRWLockExclusive(&g_Lock);
        g_Active = false;

        if (g_Patched) {
            GameCode::ApplyPatches(g_Patches, false);
            WriteLimit(c_GameLimit);
            g_Patched = false;
            g_Limit = c_GameLimit;
        }

        ReleaseSRWLockExclusive(&g_Lock);

        GameCode::Unhook({g_Node, Speedups::LookupTarget()}, g_Calls);
        g_Node = nullptr;

        if (g_Calls == 0) {
            FreeMemory();
        }
    }

    bool IsInstalled() {
        return g_Node != nullptr;
    }

    void SetEnabled(bool p_Enabled) {
        if (!g_Node) {
            return;
        }

        AcquireSRWLockExclusive(&g_Lock);

        if (p_Enabled != g_Patched) {
            GameCode::ApplyPatches(g_Patches, p_Enabled);
            g_Patched = p_Enabled;
        }

        g_Limit = p_Enabled ? c_MaxLimit : c_GameLimit;
        WriteLimit(g_Limit);
        g_Active = p_Enabled;
        ReleaseSRWLockExclusive(&g_Lock);

        if (!p_Enabled) {
            g_LastGlowing = 0;
        }
    }

    bool IsEnabled() {
        return g_Active;
    }

    uint32_t LastGlowing() {
        return g_LastGlowing;
    }

    uint32_t SpeedupsFound() {
        return g_SpeedupsFound;
    }

    uint32_t SpeedupsTotal() {
        return Speedups::c_Total;
    }
}
