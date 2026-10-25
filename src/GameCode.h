#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace GameCode {
    inline constexpr std::string_view c_GameVersion = "3.280";

    struct Section {
        const uint8_t* Begin = nullptr;
        size_t Size = 0;

        const uint8_t* End() const {
            return Begin + Size;
        }
    };

    struct Patch {
        uint8_t* Address = nullptr;
        std::vector<uint8_t> Original;
        std::vector<uint8_t> Patched;
    };

    class CallGuard {
    public:
        explicit CallGuard(std::atomic<int32_t>& p_Calls) : m_Calls(p_Calls) {
            ++m_Calls;
        }

        ~CallGuard() {
            --m_Calls;
        }

        CallGuard(const CallGuard&) = delete;
        CallGuard& operator=(const CallGuard&) = delete;

    private:
        std::atomic<int32_t>& m_Calls;
    };

    template <typename T>
    T Read(const uint8_t* p_Address) {
        T s_Value;
        memcpy(&s_Value, p_Address, sizeof(s_Value));
        return s_Value;
    }

    std::string Mismatch(std::string_view p_What);

    bool FindSection(void* p_Module, const char* p_Name, Section& p_Section);
    bool GameText(Section& p_Text);

    const uint8_t* FindUnique(const Section& p_Section, const uint8_t* p_Bytes, size_t p_Size);
    const uint8_t* FindUnique(const Section& p_Section, const int16_t* p_Pattern, size_t p_Size);
    bool MatchesAt(const uint8_t* p_Address, const int16_t* p_Pattern, size_t p_Size);
    bool Contains(const uint8_t* p_Begin, size_t p_Size, uint32_t p_Value);

    const uint8_t* RipTarget(const uint8_t* p_Instruction, size_t p_DisplacementOffset, size_t p_Length);
    bool Displacement(const uint8_t* p_InstructionEnd, const uint8_t* p_Target, int32_t& p_Displacement);

    const uint8_t* FunctionStart(const uint8_t* p_Address);
    bool FunctionBody(const uint8_t* p_Address, Section& p_Body);

    uint8_t* AllocateNear(const uint8_t* p_Code, size_t p_Size);
    void WriteCode(uint8_t* p_Address, const uint8_t* p_Bytes, size_t p_Size);
    void ApplyPatches(const std::vector<Patch>& p_Patches, bool p_Patched);

    std::string Hook(void* p_Target, void* p_Detour, void** p_Original, std::string_view p_Name);
    void Unhook(std::initializer_list<void*> p_Targets, const std::atomic<int32_t>& p_Calls);
    void WaitForCalls(const std::atomic<int32_t>& p_Calls);
}
