#include "ManyTargetsFix.h"
#include "InstinctGlow.h"
#include "TouchBufferFix.h"
#include "TrackerRays.h"

#include <Logging.h>
#include <IconsMaterialDesign.h>

#include <format>
#include <ranges>

namespace {
    std::string GhostBulletDetails() {
        return std::format("Raycast batches using a full-size buffer: {}", TouchBufferFix::BatchesFixed());
    }

    std::string RaySkipDetails() {
        return std::format(
            "Last frame: skipped {} of {} living targets", TrackerRays::LastSkipped(), TrackerRays::LastTargets()
        );
    }

    std::string InstinctDetails() {
        return std::format(
            "Glowing last frame: {}. Speedups found: {} of {}", InstinctGlow::LastGlowing(), InstinctGlow::SpeedupsFound(),
            InstinctGlow::SpeedupsTotal()
        );
    }
}

void ManyTargetsFix::OnEngineInitialized() {
    m_Parts[0] = {
        "ghost_bullet_fix", "Fix ghost bullets", &TouchBufferFix::Install, &TouchBufferFix::Uninstall,
        &TouchBufferFix::IsInstalled, &TouchBufferFix::SetEnabled, &TouchBufferFix::IsEnabled, &GhostBulletDetails
    };

    m_Parts[1] = {
        "skip_rays", "Skip target visibility rays that can't change anything", &TrackerRays::Install,
        &TrackerRays::Uninstall, &TrackerRays::IsInstalled, &TrackerRays::SetEnabled, &TrackerRays::IsEnabled,
        &RaySkipDetails
    };

    m_Parts[2] = {
        "instinct_all_targets", "Show every target in Instinct", &InstinctGlow::Install, &InstinctGlow::Uninstall,
        &InstinctGlow::IsInstalled, &InstinctGlow::SetEnabled, &InstinctGlow::IsEnabled, &InstinctDetails
    };

    for (Part& s_Part : m_Parts) {
        SetPartEnabled(s_Part, GetSettingBool("general", ZString(std::string_view(s_Part.Setting)), true));
    }
}

ManyTargetsFix::~ManyTargetsFix() {
    for (Part& s_Part : m_Parts | std::views::reverse) {
        if (s_Part.Uninstall) {
            s_Part.Uninstall();
        }
    }
}

void ManyTargetsFix::SetPartEnabled(Part& p_Part, bool p_Enabled) {
    if (p_Enabled && !p_Part.IsInstalled()) {
        if (const std::string s_Error = p_Part.Install(); !s_Error.empty()) {
            p_Part.Status = "Not active: " + s_Error;
            Logger::Warn("ManyTargetsFix: {}: {}", p_Part.Label, p_Part.Status);
            return;
        }
    }

    p_Part.SetEnabled(p_Enabled);
    p_Part.Status = p_Enabled ? "Active." : "Off.";
    Logger::Info("ManyTargetsFix: {}: {}", p_Part.Label, p_Part.Status);
}

void ManyTargetsFix::OnDrawMenu() {
    if (ImGui::Button(ICON_MD_GPS_FIXED " MANY TARGETS FIX")) {
        m_ShowWindow = !m_ShowWindow;
    }
}

void ManyTargetsFix::DrawPart(Part& p_Part) {
    bool s_Enabled = p_Part.IsEnabled();

    if (ImGui::Checkbox(p_Part.Label, &s_Enabled)) {
        SetPartEnabled(p_Part, s_Enabled);
        SetSettingBool("general", ZString(std::string_view(p_Part.Setting)), s_Enabled);
    }

    ImGui::TextDisabled("%s", p_Part.Status.c_str());

    if (p_Part.IsEnabled()) {
        ImGui::TextDisabled("%s", p_Part.Details().c_str());
    }
}

void ManyTargetsFix::OnDrawUI(bool p_HasFocus) {
    if (!m_ShowWindow) {
        return;
    }

    if (ImGui::Begin(ICON_MD_GPS_FIXED " Many Targets Fix", &m_ShowWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (size_t i = 0; i < std::size(m_Parts); ++i) {
            if (i > 0) {
                ImGui::Separator();
            }

            DrawPart(m_Parts[i]);
        }
    }

    ImGui::End();
}

DECLARE_ZHM_PLUGIN(ManyTargetsFix);
