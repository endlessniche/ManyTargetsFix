#pragma once

#include <IPluginInterface.h>

#include <string>

class ManyTargetsFix : public IPluginInterface {
public:
    void OnEngineInitialized() override;
    ~ManyTargetsFix() override;
    void OnDrawMenu() override;
    void OnDrawUI(bool p_HasFocus) override;

private:
    struct Part {
        const char* Setting = nullptr;
        const char* Label = nullptr;
        std::string (*Install)() = nullptr;
        void (*Uninstall)() = nullptr;
        bool (*IsInstalled)() = nullptr;
        void (*SetEnabled)(bool) = nullptr;
        bool (*IsEnabled)() = nullptr;
        std::string (*Details)() = nullptr;
        std::string Status = "Off.";
    };

    void SetPartEnabled(Part& p_Part, bool p_Enabled);
    void DrawPart(Part& p_Part);

private:
    bool m_ShowWindow = false;
    Part m_Parts[3];
};

DEFINE_ZHM_PLUGIN(ManyTargetsFix)
