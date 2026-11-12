#pragma once
#include "../GkGuiOverlay.hpp"

#ifdef GKMOTION_GUI_ACTIVE
namespace GakuMotion
{
    inline bool DrawStageLightingControl(StageSettings& settings, const GuiContext& ctx)
    {
        bool changed = false;
        ImGui::Spacing();
        if (!ImGui::CollapsingHeader("Live 舞台灯光（实验）")) return false;
        ImGui::BeginDisabled(!settings.lightingEffectsEnabled || settings.customNodeControl);
        const char* modes[]{"原生（不接管）", "固定染色", "柔和呼吸", "青紫 / 彩色渐变"};
        auto lighting = stage_light::Sanitize(settings.liveLighting);
        int mode = static_cast<int>(lighting.mode);
        if (ImGui::Combo("灯光效果", &mode, modes, 4))
        {
            lighting.mode = static_cast<stage_light::Mode>(mode);
            changed = true;
        }
        ImGui::BeginDisabled(lighting.mode == stage_light::Mode::Native);
        const char* palettes[]{"霓虹（青 / 紫 / 粉）", "暖色（金 / 红）", "冷色（蓝 / 青）"};
        changed |= ImGui::Combo("灯光配色", &lighting.palette, palettes, 3);
        changed |= ImGui::SliderFloat("灯光强度", &lighting.strength, 0.0f, 3.0f, "%.2fx");
        changed |= ImGui::SliderFloat("灯光 BPM", &lighting.bpm, 30.0f, 240.0f, "%.0f");
        changed |= ImGui::SliderFloat("灯光速度", &lighting.speed, 0.1f, 3.0f, "%.2fx");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (changed) settings.liveLighting = lighting;
        ImGui::TextWrapped("在已载入的通用 Live、初星和试镜舞台上驱动真实灯与体积光束。随动作时间采样，暂停冻结、跳转同步；停止或离开页面时恢复原值。");
        ImGui::TextWrapped("程序化灯光，不是 Agitato 等歌曲的原生 MV 时间轴；不新增缺失的灯具。设置自动保存。");
        if (ctx.stageLoadNotice.find("灯光设置保存失败") != std::string::npos)
            ImGui::TextWrapped("%s", ctx.stageLoadNotice.c_str());
        if (!ctx.stageLightingNotice.empty()) ImGui::TextWrapped("%s", ctx.stageLightingNotice.c_str());
        return changed;
    }
}
#endif
