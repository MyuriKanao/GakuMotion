#pragma once
#include "../GkGuiOverlay.hpp"
#include "GuiNativeLive.hpp"

#ifdef GKMOTION_GUI_ACTIVE
namespace GakuMotion
{
    // Preset or custom palette. A color drag edits a local draft and commits
    // once on release, so stage.json is not rewritten every frame.
    inline bool DrawLightingPalette(stage_light::Settings& lighting)
    {
        bool changed = false;
        const auto& presets = stage_light::palettes;
        const char* caption = lighting.palette == stage_light::kCustomPalette ? "自定义" : presets[lighting.palette].name;
        if (ImGui::BeginCombo("灯光配色", caption))
        {
            for (int i = 0; i <= stage_light::kCustomPalette; ++i)
            {
                const char* name = i == stage_light::kCustomPalette ? "自定义" : presets[i].name;
                if (ImGui::Selectable(name, lighting.palette == i) && lighting.palette != i) { lighting.palette = i; changed = true; }
                if (lighting.palette == i) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (lighting.palette != stage_light::kCustomPalette) return changed;
        static std::array<stage_light::Rgb, stage_light::kMaxCustomColors> draft{};
        static bool editing{};
        if (!editing) draft = lighting.custom;
        int count = lighting.customCount;
        if (ImGui::SliderInt("自定义颜色数", &count, 2, stage_light::kMaxCustomColors))
        { lighting.customCount = count; changed = true; }
        bool active{}, released{};
        for (int i = 0; i < lighting.customCount; ++i)
        {
            const std::string label = "颜色 " + std::to_string(i + 1);
            ImGui::ColorEdit3(label.c_str(), draft[i].data(), ImGuiColorEditFlags_NoInputs);
            active |= ImGui::IsItemActive();
            released |= ImGui::IsItemDeactivatedAfterEdit();
        }
        editing = active;
        if (released && draft != lighting.custom) { lighting.custom = draft; changed = true; }
        ImGui::TextWrapped("拖动取色时仅预览本页；松开后保存并应用到灯光。");
        return changed;
    }

    // Shown on the disabled panel (outside Photography). The template decides
    // which stage the next Photography load uses, so choosing it must not wait
    // for an active scene. Only the preference is saved here.
    inline void DrawLiveLightingPreference(const GuiContext& ctx)
    {
        if (!ImGui::CollapsingHeader("Live 舞台灯光（下次进入偶像摄影时生效）")) return;
        ImGui::PushID("live_lighting_preference");
        const char* modes[]{"原生（不接管）", "固定染色", "柔和呼吸", "青紫 / 彩色渐变", "原生模板 · 跟随曲库节拍", "通用灯光 · 跟随曲库节拍"};
        auto lighting = stage_light::Sanitize(ctx.stageSettings.liveLighting);
        bool changed = false;
        int mode = static_cast<int>(lighting.mode);
        ImGui::BeginDisabled(!ctx.fnSetLiveLightingPreference);
        if (ImGui::Combo("灯光效果", &mode, modes, 6)) { lighting.mode = static_cast<stage_light::Mode>(mode); changed = true; }
        if (lighting.mode == stage_light::Mode::SongTimeline) changed |= DrawNativeLiveSong(lighting, ctx);
        ImGui::EndDisabled();
        if (changed && ctx.fnSetLiveLightingPreference) ctx.fnSetLiveLightingPreference(ctx.self, lighting);
        ImGui::TextWrapped("这里的选择立即保存，不改动当前画面。选好原生模板后进入偶像摄影，会直接载入它的配套舞台，无需再切换一次。颜色与强度请在偶像摄影内的舞台页调整。");
        ImGui::PopID();
    }

    inline bool DrawStageLightingControl(StageSettings& settings, const GuiContext& ctx)
    {
        bool changed = false;
        ImGui::Spacing();
        if (!ImGui::CollapsingHeader("Live 舞台灯光（实验）")) return false;
        ImGui::BeginDisabled(!settings.lightingEffectsEnabled || settings.customNodeControl);
        const char* modes[]{"原生（不接管）", "固定染色", "柔和呼吸", "青紫 / 彩色渐变", "原生模板 · 跟随曲库节拍", "通用灯光 · 跟随曲库节拍"};
        auto lighting = stage_light::Sanitize(settings.liveLighting);
        int mode = static_cast<int>(lighting.mode);
        if (ImGui::Combo("灯光效果", &mode, modes, 6))
        {
            lighting.mode = static_cast<stage_light::Mode>(mode);
            changed = true;
        }
        if (lighting.mode == stage_light::Mode::SongTimeline) changed |= DrawNativeLiveSong(lighting, ctx);
        if (lighting.mode == stage_light::Mode::AutoBeat)
        {
            DrawLibraryTempo(ctx);
            changed |= ImGui::SliderFloat("灯光拍点偏移", &lighting.nativeOffset, -10.0f, 10.0f, "%.2f s");
        }
        ImGui::BeginDisabled(lighting.mode == stage_light::Mode::Native || lighting.mode == stage_light::Mode::SongTimeline);
        changed |= DrawLightingPalette(lighting);
        changed |= ImGui::SliderFloat("灯光强度", &lighting.strength, 0.0f, 3.0f, "%.2fx");
        ImGui::BeginDisabled(lighting.mode==stage_light::Mode::AutoBeat);
        changed |= ImGui::SliderFloat("灯光 BPM", &lighting.bpm, 30.0f, 240.0f, "%.0f");
        changed |= ImGui::SliderFloat("灯光速度", &lighting.speed, 0.1f, 3.0f, "%.2fx");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (changed) settings.liveLighting = lighting;
        ImGui::TextWrapped("在已载入的通用 Live、初星和试镜舞台上驱动真实灯与体积光束。随动作时间采样，暂停冻结、跳转同步；停止或离开页面时恢复原值。");
        if (lighting.mode != stage_light::Mode::SongTimeline)
            ImGui::TextWrapped("固定染色、呼吸、渐变与通用节拍是程序化灯光；原生模板模式使用原生编排并按曲库节拍重定时。设置自动保存。");
        if (ctx.stageLoadNotice.find("灯光设置保存失败") != std::string::npos)
            ImGui::TextWrapped("%s", ctx.stageLoadNotice.c_str());
        if (!ctx.stageLightingNotice.empty()) ImGui::TextWrapped("%s", ctx.stageLightingNotice.c_str());
        return changed;
    }
}
#endif
