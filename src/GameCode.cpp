#include "GameCode.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <format>

namespace GameCode {
    namespace {
        constexpr uintptr_t c_Reach = 0x7FF00000;
        constexpr int c_UnhookWaits = 200;
        constexpr DWORD c_UnhookWaitMs = 10;
    }

    std::string Mismatch(std::string_view p_What) {
        return std::format("{} didn't match (made for game version {}).", p_What, c_GameVersion);
    }

    bool FindSection(void* p_Module, const char* p_Name, Section& p_Section) {
        const auto* s_Base = static_cast<const uint8_t*>(p_Module);
        const auto* s_Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(s_Base);
        const auto* s_Nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(s_Base + s_Dos->e_lfanew);
        const auto* s_Header = IMAGE_FIRST_SECTION(s_Nt);

        for (WORD i = 0; i < s_Nt->FileHeader.NumberOfSections; ++i, ++s_Header) {
            if (strncmp(reinterpret_cast<const char*>(s_Header->Name), p_Name, IMAGE_SIZEOF_SHORT_NAME) == 0) {
                p_Section = {s_Base + s_Header->VirtualAddress, s_Header->Misc.VirtualSize};
                return true;
            }
        }

        return false;
    }

    bool GameText(Section& p_Text) {
        return FindSection(GetModuleHandleW(nullptr), ".text", p_Text);
    }

    const uint8_t* FindUnique(const Section& p_Section, const uint8_t* p_Bytes, size_t p_Size) {
        const auto* s_End = p_Section.End();
        const auto* s_First = std::search(p_Section.Begin, s_End, p_Bytes, p_Bytes + p_Size);

        if (s_First == s_End || std::search(s_First + 1, s_End, p_Bytes, p_Bytes + p_Size) != s_End) {
            return nullptr;
        }

        return s_First;
    }

    const uint8_t* FindUnique(const Section& p_Section, const int16_t* p_Pattern, size_t p_Size) {
        const uint8_t* s_Found = nullptr;

        for (const uint8_t* p = p_Section.Begin; p + p_Size <= p_Section.End(); ++p) {
            if (MatchesAt(p, p_Pattern, p_Size)) {
                if (s_Found) {
                    return nullptr;
                }

                s_Found = p;
            }
        }

        return s_Found;
    }

    bool MatchesAt(const uint8_t* p_Address, const int16_t* p_Pattern, size_t p_Size) {
        for (size_t i = 0; i < p_Size; ++i) {
            if (p_Pattern[i] >= 0 && p_Address[i] != p_Pattern[i]) {
                return false;
            }
        }

        return true;
    }

    bool Contains(const uint8_t* p_Begin, size_t p_Size, uint32_t p_Value) {
        for (const uint8_t* p = p_Begin; p + sizeof(p_Value) <= p_Begin + p_Size; ++p) {
            if (Read<uint32_t>(p) == p_Value) {
                return true;
            }
        }

        return false;
    }

    const uint8_t* RipTarget(const uint8_t* p_Instruction, size_t p_DisplacementOffset, size_t p_Length) {
        return p_Instruction + p_Length + Read<int32_t>(p_Instruction + p_DisplacementOffset);
    }

    bool Displacement(const uint8_t* p_InstructionEnd, const uint8_t* p_Target, int32_t& p_Displacement) {
        const int64_t s_Distance = reinterpret_cast<intptr_t>(p_Target) - reinterpret_cast<intptr_t>(p_InstructionEnd);

        if (s_Distance < INT32_MIN || s_Distance > INT32_MAX) {
            return false;
        }

        p_Displacement = static_cast<int32_t>(s_Distance);
        return true;
    }

    const uint8_t* FunctionStart(const uint8_t* p_Address) {
        DWORD64 s_ImageBase = 0;
        const auto* s_Function = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(p_Address), &s_ImageBase, nullptr);

        if (!s_Function) {
            return nullptr;
        }

        for (;;) {
            const auto* s_Unwind = reinterpret_cast<const uint8_t*>(s_ImageBase + s_Function->UnwindData);

            if (!((s_Unwind[0] >> 3) & UNW_FLAG_CHAININFO)) {
                break;
            }

            s_Function = reinterpret_cast<const RUNTIME_FUNCTION*>(s_Unwind + 4 + ((s_Unwind[2] + 1) & ~1) * 2);
        }

        return reinterpret_cast<const uint8_t*>(s_ImageBase + s_Function->BeginAddress);
    }

    bool FunctionBody(const uint8_t* p_Address, Section& p_Body) {
        DWORD64 s_ImageBase = 0;
        const auto* s_Function = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(p_Address), &s_ImageBase, nullptr);

        if (!s_Function) {
            return false;
        }

        p_Body = {
            reinterpret_cast<const uint8_t*>(s_ImageBase + s_Function->BeginAddress),
            static_cast<size_t>(s_Function->EndAddress - s_Function->BeginAddress)
        };

        return true;
    }

    uint8_t* AllocateNear(const uint8_t* p_Code, size_t p_Size) {
        SYSTEM_INFO s_System;
        GetSystemInfo(&s_System);
        const uintptr_t s_Granularity = s_System.dwAllocationGranularity;
        const uintptr_t s_Code = reinterpret_cast<uintptr_t>(p_Code);
        const uintptr_t s_Low = s_Code > c_Reach ? s_Code - c_Reach : s_Granularity;
        const uintptr_t s_High = s_Code + c_Reach;

        for (uintptr_t s_Address = s_Code; s_Address < s_High;) {
            MEMORY_BASIC_INFORMATION s_Info;

            if (!VirtualQuery(reinterpret_cast<void*>(s_Address), &s_Info, sizeof(s_Info))) {
                break;
            }

            const uintptr_t s_RegionStart = reinterpret_cast<uintptr_t>(s_Info.BaseAddress);
            const uintptr_t s_RegionEnd = s_RegionStart + s_Info.RegionSize;

            if (s_Info.State == MEM_FREE) {
                const uintptr_t s_Start = (s_RegionStart + s_Granularity - 1) / s_Granularity * s_Granularity;

                if (s_Start + p_Size <= s_RegionEnd && s_Start + p_Size <= s_High) {
                    if (void* s_Memory = VirtualAlloc(
                        reinterpret_cast<void*>(s_Start), p_Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
                    )) {
                        return static_cast<uint8_t*>(s_Memory);
                    }
                }
            }

            s_Address = s_RegionEnd;
        }

        for (uintptr_t s_Address = s_Code; s_Address > s_Low;) {
            MEMORY_BASIC_INFORMATION s_Info;

            if (!VirtualQuery(reinterpret_cast<void*>(s_Address), &s_Info, sizeof(s_Info))) {
                break;
            }

            const uintptr_t s_RegionStart = reinterpret_cast<uintptr_t>(s_Info.AllocationBase ? s_Info.AllocationBase : s_Info.BaseAddress);

            if (s_Info.State == MEM_FREE) {
                const uintptr_t s_RegionEnd = reinterpret_cast<uintptr_t>(s_Info.BaseAddress) + s_Info.RegionSize;
                const uintptr_t s_Start = (s_RegionEnd - p_Size) / s_Granularity * s_Granularity;

                if (s_Start >= reinterpret_cast<uintptr_t>(s_Info.BaseAddress) && s_Start >= s_Low) {
                    if (void* s_Memory = VirtualAlloc(
                        reinterpret_cast<void*>(s_Start), p_Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE
                    )) {
                        return static_cast<uint8_t*>(s_Memory);
                    }
                }
            }

            if (s_RegionStart <= s_Granularity) {
                break;
            }

            s_Address = s_RegionStart - 1;
        }

        return nullptr;
    }

    void WriteCode(uint8_t* p_Address, const uint8_t* p_Bytes, size_t p_Size) {
        DWORD s_OldProtect;
        VirtualProtect(p_Address, p_Size, PAGE_EXECUTE_READWRITE, &s_OldProtect);
        memcpy(p_Address, p_Bytes, p_Size);
        VirtualProtect(p_Address, p_Size, s_OldProtect, &s_OldProtect);
        FlushInstructionCache(GetCurrentProcess(), p_Address, p_Size);
    }

    void ApplyPatches(const std::vector<Patch>& p_Patches, bool p_Patched) {
        for (const Patch& s_Patch : p_Patches) {
            const auto& s_Bytes = p_Patched ? s_Patch.Patched : s_Patch.Original;
            WriteCode(s_Patch.Address, s_Bytes.data(), s_Bytes.size());
        }
    }

    std::string Hook(void* p_Target, void* p_Detour, void** p_Original, std::string_view p_Name) {
        const MH_STATUS s_Init = MH_Initialize();

        if (s_Init != MH_OK && s_Init != MH_ERROR_ALREADY_INITIALIZED) {
            return std::format("MinHook could not start ({}).", MH_StatusToString(s_Init));
        }

        MH_STATUS s_Status = MH_CreateHook(p_Target, p_Detour, p_Original);

        if (s_Status == MH_OK) {
            s_Status = MH_EnableHook(p_Target);
        }

        if (s_Status != MH_OK) {
            MH_RemoveHook(p_Target);
            return std::format("{} could not be hooked ({}).", p_Name, MH_StatusToString(s_Status));
        }

        return {};
    }

    void WaitForCalls(const std::atomic<int32_t>& p_Calls) {
        for (int i = 0; i < c_UnhookWaits && p_Calls > 0; ++i) {
            Sleep(c_UnhookWaitMs);
        }
    }

    void Unhook(std::initializer_list<void*> p_Targets, const std::atomic<int32_t>& p_Calls) {
        for (void* s_Target : p_Targets) {
            if (s_Target) {
                MH_DisableHook(s_Target);
            }
        }

        WaitForCalls(p_Calls);

        for (void* s_Target : p_Targets) {
            if (s_Target) {
                MH_RemoveHook(s_Target);
            }
        }
    }
}
