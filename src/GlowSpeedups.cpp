#include "GlowSpeedups.h"
#include "GlowState.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <cstring>
#include <iterator>

namespace InstinctGlow::Speedups {
    namespace {
        constexpr int16_t c_BoneUpload[] = {
            0x48, 0x8B, 0x44, 0x24, 0x68, 0x48, 0x8D, 0x54, 0x24, 0x68, 0x48, 0x05, 0x10, 0x01, 0x00, 0x00,
            0x44, 0x8B, 0xC3, 0x48, 0x89, 0x45, 0x90, 0x48, 0x8B, 0xCF, 0x33, 0xC0, 0x48, 0x89, 0x44, 0x24,
            0x68, 0x48, 0x89, 0x45, 0xA0, 0xE8, -1, -1, -1, -1, 0x44, 0x8B, 0xC3, 0x49, 0x8D, 0x94,
            0x24, 0x10, 0x01, 0x00, 0x00, 0x48, 0x8B, 0xC8, 0xE8, -1, -1, -1, -1, 0x48, 0x8B, 0x5C,
            0x24, 0x68, 0x48, 0x85, 0xDB, 0x74, 0x07, 0x81, 0x63, 0x08, 0xFF, 0xFF, 0xFF, 0xBF, 0x44, 0x8B,
            0x65, 0x80, 0x48, 0x8D, 0x55, 0xA0, 0x45, 0x8B, 0xC4, 0x48, 0x8B, 0xCF, 0xE8, -1, -1, -1,
            -1, 0x48, 0x8B, 0x55, 0x90, 0x45, 0x8B, 0xC4, 0x48, 0x8B, 0xC8, 0xE8, -1, -1, -1, -1,
        };

        constexpr size_t c_BoneAllocCalls[] = {37, 92};
        constexpr size_t c_BoneCopyCalls[] = {56, 107};

        constexpr int16_t c_TransientAlloc[] = {
            0x48, 0x89, 0x6C, 0x24, 0x18, 0x56, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xF2, 0x48, 0x8B, 0xE9,
            0x45, 0x85, 0xC0, 0x75, 0x14, 0x48, 0xC7, 0x02, 0x00, 0x00, 0x00, 0x00, 0x33, 0xC0, 0x48, 0x8B,
            0x6C, 0x24, 0x40, 0x48, 0x83, 0xC4, 0x20, 0x5E, 0xC3, 0x48, 0x89, 0x5C, 0x24, 0x30, 0x48, 0x81,
            0xC1, 0xC0, 0x13, 0x00, 0x00, 0x48, 0x89, 0x7C, 0x24, 0x38, 0x41, 0x8D, 0x78, 0x0F, 0x83, 0xE7,
            0xF0, 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x8D, 0x57, 0x10, 0xE8, -1, -1, -1, -1, 0x48,
            0x8D, 0x8D, 0xE0, 0x11, 0x00, 0x00, 0x41, 0xB8, 0x00, 0x01, 0x00, 0x00, 0x8B, 0xD7, 0x48, 0x8B,
            0xD8, 0xE8, -1, -1, -1, -1, 0x48, 0x8B, 0x6C, 0x24, 0x40, 0x81, 0xE7, 0xFF, 0xFF, 0xFF,
            0x1F, 0x48, 0x89, 0x03, 0x81, 0xCF, 0x00, 0x00, 0x00, 0x60, 0x89, 0x7B, 0x08, 0x48, 0x8B, 0x7C,
            0x24, 0x38, 0x48, 0x89, 0x1E, 0x48, 0x8B, 0x03, 0x48, 0x8B, 0x5C, 0x24, 0x30, 0x48, 0x83, 0xC4,
            0x20, 0x5E, 0xC3,
        };

        constexpr uint8_t c_PaletteCall[] = {
            0xF3, 0x41, 0x0F, 0x10, 0x9F, 0xB4, 0x00, 0x00, 0x00, 0x4D, 0x8B, 0xC4, 0x48, 0x8B, 0xD3, 0x48,
            0x8B, 0xCF, 0xE8,
        };

        constexpr uint8_t c_BindCall[] = {0x4C, 0x8B, 0xC3, 0xBA, 0x04, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCF, 0xE8};
        constexpr size_t c_BindCallOffset = 0xFD;
        constexpr int32_t c_PaletteSlot = 4;
        constexpr int32_t c_PreviousPaletteSlot = 6;
        constexpr size_t c_BoundBuffers = 0xDF8;
        constexpr size_t c_DisplacementWindow = 0x800;

        constexpr uint8_t c_LookupSignature[] = {
            0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xC1, 0x33, 0xC9, 0x44, 0x8B, 0xC9, 0xF0, 0x44,
            0x0F, 0xC1, 0x88, 0x70, 0x14, 0x03, 0x00, 0x44, 0x8B, 0xC1, 0x45, 0x85, 0xC9, 0x74, 0x2D, 0x48,
            0x05, 0x78, 0x14, 0x03, 0x00, 0x66, 0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0xDA, 0x48, 0x2B, 0x58, 0x08, 0x48, 0x81, 0xFB, 0xFF, 0xFF, 0xFF, 0x03, 0x76, 0x1E,
            0x41, 0xFF, 0xC0, 0x48, 0x83, 0xC0, 0x20, 0x45, 0x3B, 0xC1, 0x72, 0xE4, 0x48, 0x8B, 0x01, 0x48,
            0x8B, 0xD9, 0xFF, 0x50, 0x58, 0x48, 0x03, 0xC3, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3, 0x48, 0x8B,
            0x08, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x58, 0x48, 0x03, 0xC3, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3,
        };

        constexpr size_t c_HeapCountOffset = 0x31470;
        constexpr size_t c_HeapEntriesOffset = 0x31478;
        constexpr size_t c_HeapEntrySize = 0x20;
        constexpr size_t c_HeapEntryBase = 0x08;
        constexpr uint64_t c_HeapChunkSize = 0x4000000;
        constexpr size_t c_GpuAddressSlot = 0x58;
        constexpr size_t c_HeapCacheSize = 8;

        constexpr uint8_t c_LinkSearch[] = {
            0x8B, 0xC7, 0x85, 0xF6, 0x74, 0x62, 0x48, 0x8D, 0x8D, 0x40, 0x03, 0x00, 0x00, 0x66, 0x90, 0x48,
            0x39, 0x19, 0x74, 0x0C, 0xFF, 0xC0, 0x48, 0x83, 0xC1, 0x20, 0x3B, 0xC6, 0x72, 0xF1, 0xEB, 0x48,
            0x48, 0xC1, 0xE0, 0x05, 0x48, 0x8D, 0x95, 0x30, 0x03, 0x00, 0x00, 0x48, 0x03, 0xD0, 0x74, 0x38,
            0x8B, 0x8D, 0xA0, 0x00, 0x00, 0x00, 0x0F, 0x57, 0xC0, 0x48, 0xC1, 0xE1, 0x04, 0x48, 0x03, 0x8D,
            0x90, 0x00, 0x00, 0x00, 0x0F, 0x11, 0x01, 0xFF, 0x85, 0xA0, 0x00, 0x00, 0x00, 0x4C, 0x89, 0x21,
            0x48, 0x8B, 0x42, 0x18, 0x48, 0x89, 0x41, 0x08, 0x48, 0x89, 0x4A, 0x18, 0x4D, 0x85, 0xFF, 0x74,
            0x07, 0xC7, 0x42, 0x08, 0x01, 0x00, 0x00, 0x00,
        };

        constexpr uint8_t c_LinkStubSave[] = {
            0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x48, 0x81, 0xEC, 0x80, 0x00, 0x00,
            0x00, 0x0F, 0x29, 0x44, 0x24, 0x20, 0x0F, 0x29, 0x4C, 0x24, 0x30, 0x0F, 0x29, 0x54, 0x24, 0x40,
            0x0F, 0x29, 0x5C, 0x24, 0x50, 0x0F, 0x29, 0x64, 0x24, 0x60, 0x0F, 0x29, 0x6C, 0x24, 0x70, 0x48,
            0x8B, 0xCB, 0x8B, 0xD6, 0x48, 0xB8,
        };

        constexpr uint8_t c_LinkStubRestore[] = {
            0xFF, 0xD0, 0x8B, 0xC0, 0x0F, 0x28, 0x44, 0x24, 0x20, 0x0F, 0x28, 0x4C, 0x24, 0x30, 0x0F, 0x28,
            0x54, 0x24, 0x40, 0x0F, 0x28, 0x5C, 0x24, 0x50, 0x0F, 0x28, 0x64, 0x24, 0x60, 0x0F, 0x28, 0x6C,
            0x24, 0x70, 0x48, 0x81, 0xC4, 0x80, 0x00, 0x00, 0x00, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41,
            0x58, 0x5A, 0x59, 0x83, 0xF8, 0xFF,
        };

        constexpr size_t c_LinkSearchPatched = 0x20;
        constexpr size_t c_LinkFoundOffset = 0x20;
        constexpr size_t c_LinkMissingOffset = 0x68;
        constexpr size_t c_LinkTableSize = 4096;
        constexpr uint32_t c_NotFound = UINT32_MAX;

        constexpr size_t c_AllocThunkOffset = 0x200;
        constexpr size_t c_CopyThunkOffset = 0x210;
        constexpr size_t c_PaletteThunkOffset = 0x220;
        constexpr size_t c_LinkStubOffset = 0x300;
        constexpr uint8_t c_Thunk[] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
        constexpr size_t c_CacheSize = 4096;
        constexpr size_t c_CacheProbes = 64;

        using AllocFn = uint64_t(*)(uint64_t, uint64_t*, int32_t);
        using CopyFn = void*(*)(void*, const void*, size_t);
        using PaletteFn = void(*)(uint64_t, uint64_t, uint64_t, float);
        using BindFn = void(*)(uint64_t, int32_t, uint64_t);
        using LookupFn = uint64_t(*)(uint64_t, uint64_t);
        using GpuAddressFn = uint64_t(*)(uint64_t);

        struct BoneUpload {
            const void* Source = nullptr;
            uint64_t Context = 0;
            size_t Size = 0;
            uint64_t Handle = 0;
            uint32_t NodeCall = 0;
        };

        struct PaletteUpload {
            uint64_t Context = 0;
            uint64_t Palette = 0;
            uint64_t Previous = 0;
            uint32_t Scale = 0;
            uint32_t NodeCall = 0;
            uint64_t Bound = 0;
            uint64_t PreviousBound = 0;
        };

        struct HeapChunk {
            uint64_t Manager = 0;
            uint64_t Base = 0;
            uint64_t Gpu = 0;
            uint32_t NodeCall = 0;
        };

        struct LinkSlot {
            uint64_t Key = 0;
            uint32_t Index = 0;
            uint32_t Generation = 0;
        };

        AllocFn g_Alloc = nullptr;
        CopyFn g_Copy = nullptr;
        PaletteFn g_Palette = nullptr;
        BindFn g_Bind = nullptr;
        BoneUpload g_BoneCache[c_CacheSize];
        PaletteUpload g_PaletteCache[c_CacheSize];

        alignas(16) uint8_t g_DeferredMemory[16];
        bool g_Deferred = false;
        uint64_t g_DeferredContext = 0;
        uint64_t* g_DeferredHandle = nullptr;
        int32_t g_DeferredSize = 0;

        uint8_t* g_Lookup = nullptr;
        LookupFn g_LookupOriginal = nullptr;
        HeapChunk g_HeapCache[c_HeapCacheSize];
        uint32_t g_HeapNext = 0;

        LinkSlot g_LinkTable[c_LinkTableSize];
        uint32_t g_LinkGeneration = 0;
        uint32_t g_LinkCount = 0;
        uint32_t g_LinkBuiltFor = 0;
        bool g_LinkTableFull = false;

        bool OnNodeThread() {
            return g_Active && g_NodeThread == GetCurrentThreadId();
        }

        size_t HashIndex(uint64_t p_Key, size_t p_TableSize) {
            return static_cast<size_t>(((p_Key >> 4) * 0x9E3779B97F4A7C15ULL) >> 52) & (p_TableSize - 1);
        }

        BoneUpload* FindBoneUpload(const void* p_Source, uint64_t p_Context, size_t p_Size, bool& p_Found) {
            size_t s_Index = HashIndex(reinterpret_cast<uint64_t>(p_Source), c_CacheSize);

            for (size_t i = 0; i < c_CacheProbes; ++i, s_Index = (s_Index + 1) & (c_CacheSize - 1)) {
                BoneUpload& s_Slot = g_BoneCache[s_Index];

                if (s_Slot.NodeCall != g_NodeCall) {
                    p_Found = false;
                    return &s_Slot;
                }

                if (s_Slot.Source == p_Source && s_Slot.Context == p_Context && s_Slot.Size == p_Size) {
                    p_Found = true;
                    return &s_Slot;
                }
            }

            p_Found = false;
            return nullptr;
        }

        uint64_t DeferAlloc(uint64_t p_Context, uint64_t* p_Handle, int32_t p_Size) {
            if (p_Size <= 0 || !OnNodeThread()) {
                return g_Alloc(p_Context, p_Handle, p_Size);
            }

            g_Deferred = true;
            g_DeferredContext = p_Context;
            g_DeferredHandle = p_Handle;
            g_DeferredSize = p_Size;
            *p_Handle = 0;
            return reinterpret_cast<uint64_t>(g_DeferredMemory);
        }

        void* CopyShared(void* p_Destination, const void* p_Source, size_t p_Size) {
            if (p_Destination != g_DeferredMemory || !g_Deferred) {
                return g_Copy(p_Destination, p_Source, p_Size);
            }

            g_Deferred = false;
            bool s_Found = false;
            BoneUpload* s_Slot = FindBoneUpload(p_Source, g_DeferredContext, p_Size, s_Found);

            if (s_Found) {
                *g_DeferredHandle = s_Slot->Handle;
                return p_Destination;
            }

            void* s_Memory = reinterpret_cast<void*>(g_Alloc(g_DeferredContext, g_DeferredHandle, g_DeferredSize));
            g_Copy(s_Memory, p_Source, p_Size);

            if (s_Slot) {
                *s_Slot = {p_Source, g_DeferredContext, p_Size, *g_DeferredHandle, g_NodeCall};
            }

            return s_Memory;
        }

        void SharePalette(uint64_t p_Context, uint64_t p_Palette, uint64_t p_Previous, float p_Scale) {
            if (!OnNodeThread()) {
                g_Palette(p_Context, p_Palette, p_Previous, p_Scale);
                return;
            }

            uint32_t s_Scale = 0;
            uint64_t s_Previous = 0;

            if (p_Palette) {
                memcpy(&s_Scale, &p_Scale, sizeof(s_Scale));
                s_Previous = p_Previous;
            }

            size_t s_Index = HashIndex(p_Palette ^ (s_Previous << 1), c_CacheSize);
            PaletteUpload* s_Free = nullptr;

            for (size_t i = 0; i < c_CacheProbes; ++i, s_Index = (s_Index + 1) & (c_CacheSize - 1)) {
                PaletteUpload& s_Slot = g_PaletteCache[s_Index];

                if (s_Slot.NodeCall != g_NodeCall) {
                    s_Free = &s_Slot;
                    break;
                }

                if (s_Slot.Context == p_Context && s_Slot.Palette == p_Palette && s_Slot.Previous == s_Previous &&
                    s_Slot.Scale == s_Scale) {
                    g_Bind(p_Context, c_PaletteSlot, s_Slot.Bound);
                    g_Bind(p_Context, c_PreviousPaletteSlot, s_Slot.PreviousBound);
                    return;
                }
            }

            g_Palette(p_Context, p_Palette, p_Previous, p_Scale);

            if (s_Free) {
                const auto* s_Bound = reinterpret_cast<const uint64_t*>(p_Context + c_BoundBuffers);
                *s_Free = {
                    p_Context, p_Palette, s_Previous, s_Scale, g_NodeCall, s_Bound[c_PaletteSlot],
                    s_Bound[c_PreviousPaletteSlot]
                };
            }
        }

        uint64_t Detour_Lookup(uint64_t p_Manager, uint64_t p_Address) {
            GameCode::CallGuard s_Guard(g_Calls);

            if (!OnNodeThread()) {
                return g_LookupOriginal(p_Manager, p_Address);
            }

            for (const HeapChunk& s_Chunk : g_HeapCache) {
                if (s_Chunk.NodeCall == g_NodeCall && s_Chunk.Manager == p_Manager &&
                    p_Address - s_Chunk.Base < c_HeapChunkSize) {
                    return s_Chunk.Gpu + (p_Address - s_Chunk.Base);
                }
            }

            const auto* s_Manager = reinterpret_cast<const uint8_t*>(p_Manager);
            const auto s_Count = static_cast<uint32_t>(
                _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(p_Manager + c_HeapCountOffset), 0)
            );

            for (uint32_t i = 0; i < s_Count; ++i) {
                const uint8_t* s_Entry = s_Manager + c_HeapEntriesOffset + static_cast<size_t>(i) * c_HeapEntrySize;
                const auto s_Base = GameCode::Read<uint64_t>(s_Entry + c_HeapEntryBase);

                if (p_Address - s_Base < c_HeapChunkSize) {
                    const auto s_Resource = GameCode::Read<uint64_t>(s_Entry);
                    const auto* s_Vtable = reinterpret_cast<const uint8_t*>(GameCode::Read<uint64_t>(reinterpret_cast<const uint8_t*>(s_Resource)));
                    const auto s_GpuAddress = reinterpret_cast<GpuAddressFn>(GameCode::Read<uint64_t>(s_Vtable + c_GpuAddressSlot));
                    const uint64_t s_Gpu = s_GpuAddress(s_Resource);

                    g_HeapCache[g_HeapNext] = {p_Manager, s_Base, s_Gpu, g_NodeCall};
                    g_HeapNext = (g_HeapNext + 1) % c_HeapCacheSize;
                    return s_Gpu + (p_Address - s_Base);
                }
            }

            return g_LookupOriginal(p_Manager, p_Address);
        }

        uint32_t ScanLinkEntry(uint64_t p_Key, uint32_t p_Count) {
            for (uint32_t i = 0; i < p_Count; ++i) {
                if (g_List[i].Key == p_Key) {
                    return i;
                }
            }

            return c_NotFound;
        }

        void BuildLinkTable(uint32_t p_Count) {
            if (++g_LinkGeneration == 0) {
                for (LinkSlot& s_Slot : g_LinkTable) {
                    s_Slot.Generation = 0;
                }

                g_LinkGeneration = 1;
            }

            g_LinkCount = p_Count;
            g_LinkTableFull = false;

            for (uint32_t i = 0; i < p_Count && !g_LinkTableFull; ++i) {
                const uint64_t s_Key = g_List[i].Key;
                size_t s_Index = HashIndex(s_Key, c_LinkTableSize);
                size_t s_Probes = 0;

                while (g_LinkTable[s_Index].Generation == g_LinkGeneration && g_LinkTable[s_Index].Key != s_Key) {
                    s_Index = (s_Index + 1) & (c_LinkTableSize - 1);

                    if (++s_Probes == c_LinkTableSize) {
                        g_LinkTableFull = true;
                        break;
                    }
                }

                if (!g_LinkTableFull && g_LinkTable[s_Index].Generation != g_LinkGeneration) {
                    g_LinkTable[s_Index] = {s_Key, i, g_LinkGeneration};
                }
            }
        }

        uint32_t FindLinkEntry(uint64_t p_Key, uint32_t p_Count) {
            if (p_Count == 0) {
                return c_NotFound;
            }

            if (!g_Active) {
                return ScanLinkEntry(p_Key, p_Count);
            }

            if (g_LinkBuiltFor != g_NodeCall || g_LinkCount != p_Count) {
                BuildLinkTable(p_Count);
                g_LinkBuiltFor = g_NodeCall;
            }

            if (g_LinkTableFull) {
                return ScanLinkEntry(p_Key, p_Count);
            }

            size_t s_Index = HashIndex(p_Key, c_LinkTableSize);

            for (size_t s_Probes = 0; s_Probes < c_LinkTableSize; ++s_Probes) {
                const LinkSlot& s_Slot = g_LinkTable[s_Index];

                if (s_Slot.Generation != g_LinkGeneration) {
                    return c_NotFound;
                }

                if (s_Slot.Key == p_Key) {
                    return s_Slot.Index;
                }

                s_Index = (s_Index + 1) & (c_LinkTableSize - 1);
            }

            return c_NotFound;
        }

        void WriteThunk(uint8_t* p_Thunk, const void* p_Target) {
            const uint64_t s_Target = reinterpret_cast<uint64_t>(p_Target);
            memcpy(p_Thunk, c_Thunk, sizeof(c_Thunk));
            memcpy(p_Thunk + sizeof(c_Thunk), &s_Target, sizeof(s_Target));
        }

        bool RedirectCall(const uint8_t* p_Call, uint8_t* p_Thunk, std::vector<GameCode::Patch>& p_Patches) {
            int32_t s_Into;

            if (p_Call[0] != 0xE8 || !GameCode::Displacement(p_Call + 5, p_Thunk, s_Into)) {
                return false;
            }

            GameCode::Patch s_Patch;
            s_Patch.Address = const_cast<uint8_t*>(p_Call + 1);
            s_Patch.Original.assign(p_Call + 1, p_Call + 5);
            s_Patch.Patched.resize(sizeof(s_Into));
            memcpy(s_Patch.Patched.data(), &s_Into, sizeof(s_Into));
            p_Patches.push_back(std::move(s_Patch));
            return true;
        }

        bool PrepareLinkTable(const GameCode::Section& p_Node, uint8_t* p_Stub, std::vector<GameCode::Patch>& p_Patches) {
            const uint8_t* s_Search = GameCode::FindUnique(p_Node, c_LinkSearch, sizeof(c_LinkSearch));

            if (!s_Search) {
                return false;
            }

            uint8_t* s_Stub = p_Stub + c_LinkStubOffset;
            const uint64_t s_Find = reinterpret_cast<uint64_t>(&FindLinkEntry);
            const auto* s_FindBytes = reinterpret_cast<const uint8_t*>(&s_Find);

            std::vector<uint8_t> s_Code(std::begin(c_LinkStubSave), std::end(c_LinkStubSave));
            s_Code.insert(s_Code.end(), s_FindBytes, s_FindBytes + sizeof(s_Find));
            s_Code.insert(s_Code.end(), std::begin(c_LinkStubRestore), std::end(c_LinkStubRestore));

            int32_t s_Missing;
            int32_t s_Found;
            int32_t s_Into;
            const size_t s_MissingEnd = s_Code.size() + 6;
            const size_t s_FoundEnd = s_MissingEnd + 5;

            if (!GameCode::Displacement(s_Stub + s_MissingEnd, s_Search + c_LinkMissingOffset, s_Missing) ||
                !GameCode::Displacement(s_Stub + s_FoundEnd, s_Search + c_LinkFoundOffset, s_Found) ||
                !GameCode::Displacement(s_Search + 5, s_Stub, s_Into)) {
                return false;
            }

            s_Code.push_back(0x0F);
            s_Code.push_back(0x84);
            s_Code.resize(s_Code.size() + sizeof(s_Missing));
            memcpy(s_Code.data() + s_Code.size() - sizeof(s_Missing), &s_Missing, sizeof(s_Missing));
            s_Code.push_back(0xE9);
            s_Code.resize(s_Code.size() + sizeof(s_Found));
            memcpy(s_Code.data() + s_Code.size() - sizeof(s_Found), &s_Found, sizeof(s_Found));
            memcpy(s_Stub, s_Code.data(), s_Code.size());

            GameCode::Patch s_Patch;
            s_Patch.Address = const_cast<uint8_t*>(s_Search);
            s_Patch.Original.assign(s_Search, s_Search + c_LinkSearchPatched);
            s_Patch.Patched = {0xE9, 0, 0, 0, 0};
            memcpy(s_Patch.Patched.data() + 1, &s_Into, sizeof(s_Into));
            s_Patch.Patched.resize(c_LinkSearchPatched, 0x90);
            p_Patches.push_back(std::move(s_Patch));
            return true;
        }

        bool PrepareSkeletonSharing(uint8_t* p_Stub, std::vector<GameCode::Patch>& p_Patches) {
            GameCode::Section s_Text;

            if (!GameCode::GameText(s_Text)) {
                return false;
            }

            const uint8_t* s_Block = GameCode::FindUnique(s_Text, c_BoneUpload, std::size(c_BoneUpload));
            GameCode::Section s_Draw;

            if (!s_Block || !GameCode::FunctionBody(s_Block, s_Draw)) {
                return false;
            }

            const uint8_t* s_Alloc = GameCode::RipTarget(s_Block + c_BoneAllocCalls[0], 1, 5);
            const uint8_t* s_Copy = GameCode::RipTarget(s_Block + c_BoneCopyCalls[0], 1, 5);

            if (GameCode::RipTarget(s_Block + c_BoneAllocCalls[1], 1, 5) != s_Alloc ||
                GameCode::RipTarget(s_Block + c_BoneCopyCalls[1], 1, 5) != s_Copy ||
                !GameCode::MatchesAt(s_Alloc, c_TransientAlloc, std::size(c_TransientAlloc)) ||
                memcmp(s_Block + c_BindCallOffset, c_BindCall, sizeof(c_BindCall)) != 0) {
                return false;
            }

            const uint8_t* s_BindFunction = GameCode::RipTarget(s_Block + c_BindCallOffset + sizeof(c_BindCall) - 1, 1, 5);
            const uint8_t* s_PaletteCall = GameCode::FindUnique(s_Draw, c_PaletteCall, sizeof(c_PaletteCall));

            if (!s_PaletteCall ||
                !GameCode::Contains(s_BindFunction, c_DisplacementWindow, static_cast<uint32_t>(c_BoundBuffers))) {
                return false;
            }

            const uint8_t* s_PaletteCallInstruction = s_PaletteCall + sizeof(c_PaletteCall) - 1;
            const uint8_t* s_PaletteFunction = GameCode::RipTarget(s_PaletteCallInstruction, 1, 5);
            const auto s_PaletteBinding = static_cast<uint32_t>(c_BoundBuffers + c_PaletteSlot * sizeof(uint64_t));
            const auto s_PreviousBinding = static_cast<uint32_t>(c_BoundBuffers + c_PreviousPaletteSlot * sizeof(uint64_t));

            if (!GameCode::Contains(s_PaletteFunction, c_DisplacementWindow, s_PaletteBinding) ||
                !GameCode::Contains(s_PaletteFunction, c_DisplacementWindow, s_PreviousBinding)) {
                return false;
            }

            uint8_t* s_AllocThunk = p_Stub + c_AllocThunkOffset;
            uint8_t* s_CopyThunk = p_Stub + c_CopyThunkOffset;
            uint8_t* s_PaletteThunk = p_Stub + c_PaletteThunkOffset;
            std::vector<GameCode::Patch> s_Patches;

            if (!RedirectCall(s_PaletteCallInstruction, s_PaletteThunk, s_Patches)) {
                return false;
            }

            for (const size_t s_Offset : c_BoneAllocCalls) {
                if (!RedirectCall(s_Block + s_Offset, s_AllocThunk, s_Patches)) {
                    return false;
                }
            }

            for (const size_t s_Offset : c_BoneCopyCalls) {
                if (!RedirectCall(s_Block + s_Offset, s_CopyThunk, s_Patches)) {
                    return false;
                }
            }

            g_Alloc = reinterpret_cast<AllocFn>(s_Alloc);
            g_Copy = reinterpret_cast<CopyFn>(s_Copy);
            g_Palette = reinterpret_cast<PaletteFn>(s_PaletteFunction);
            g_Bind = reinterpret_cast<BindFn>(s_BindFunction);
            WriteThunk(s_AllocThunk, reinterpret_cast<const void*>(&DeferAlloc));
            WriteThunk(s_CopyThunk, reinterpret_cast<const void*>(&CopyShared));
            WriteThunk(s_PaletteThunk, reinterpret_cast<const void*>(&SharePalette));
            p_Patches.insert(p_Patches.end(), s_Patches.begin(), s_Patches.end());
            return true;
        }
    }

    uint32_t Prepare(const GameCode::Section& p_Node, uint8_t* p_Stub, std::vector<GameCode::Patch>& p_Patches) {
        uint32_t s_Found = 0;
        s_Found += PrepareSkeletonSharing(p_Stub, p_Patches);
        s_Found += PrepareLinkTable(p_Node, p_Stub, p_Patches);
        return s_Found;
    }

    bool HookLookup() {
        GameCode::Section s_Text;

        if (g_Lookup || !GameCode::GameText(s_Text)) {
            return g_Lookup != nullptr;
        }

        const uint8_t* s_Lookup = GameCode::FindUnique(s_Text, c_LookupSignature, sizeof(c_LookupSignature));

        if (!s_Lookup) {
            return false;
        }

        auto* s_Target = const_cast<uint8_t*>(s_Lookup);

        if (!GameCode::Hook(s_Target, reinterpret_cast<void*>(&Detour_Lookup), reinterpret_cast<void**>(&g_LookupOriginal), "The GPU address lookup").empty()) {
            return false;
        }

        g_Lookup = s_Target;
        return true;
    }

    void* LookupTarget() {
        return g_Lookup;
    }

    void BeginNodeCall() {
        if (++g_NodeCall == 0) {
            for (BoneUpload& s_Slot : g_BoneCache) {
                s_Slot.NodeCall = 0;
            }

            for (PaletteUpload& s_Slot : g_PaletteCache) {
                s_Slot.NodeCall = 0;
            }

            for (HeapChunk& s_Chunk : g_HeapCache) {
                s_Chunk.NodeCall = 0;
            }

            g_NodeCall = 1;
        }

        g_Deferred = false;
    }

    void EndNodeCall() {
        g_Deferred = false;
    }
}
