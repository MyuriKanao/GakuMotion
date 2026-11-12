#pragma once
#ifdef GKMOTION_GUI_ACTIVE
namespace GakuMotion
{
    inline bool DrawNativeLiveSong(stage_light::Settings& lighting, const GuiContext& ctx)
    {
        bool changed = false;
        const auto songs = native_live::catalog.Snapshot();
        const auto selected = native_live::catalog.Find(lighting.songKey);
        auto labelOf = [](const native_live::Song& song) {
            return song.title + " / " + (song.variant.empty() ? "演出版本 " + std::to_string(song.grade) : song.variant);
        };
        const auto caption = selected ? labelOf(*selected) : songs.empty() ? "目录未就绪，请刷新本地曲目" : "请选择歌曲";
        ImGui::BeginDisabled(songs.empty());
        if (ImGui::BeginCombo("原生歌曲 / 版本", caption.c_str()))
        {
            for (const auto& song : songs)
            {
                const auto label = labelOf(song) + "##" + song.key;
                if (ImGui::Selectable(label.c_str(), song.key == lighting.songKey))
                { lighting.songKey = song.key; changed = true; }
                if (song.key == lighting.songKey) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        changed |= ImGui::SliderFloat("原生时间轴偏移", &lighting.nativeOffset, -10.0f, 10.0f, "%.2f s");
        if (ImGui::Button("刷新本地曲目")) native_live::refreshRequested.store(true);
        ImGui::SameLine();
        if (ImGui::Button("重试原生灯光")) native_live::retryRequested.store(true);
        if (selected)
        {
            ImGui::TextWrapped("配套舞台：%s", selected->stage.c_str());
            ImGui::TextWrapped("原生时间轴：%s", selected->timeline.c_str());
        }
        if (!ctx.nativeLiveCatalogNotice.empty()) ImGui::TextWrapped("%s", ctx.nativeLiveCatalogNotice.c_str());
        ImGui::TextWrapped("先选歌曲与演出版本，再重新进入摄影加载配套舞台。只加载已缓存资源；歌曲/舞台不匹配时保持待机，不使用 Agitato 灯控代替其他歌曲。");
        ImGui::TextWrapped("仅复用原生灯具、激光、特效、荧光棒及 BPM 轨；外部动作与当前镜头不接管。以动作绝对时间采样，暂停保持、跳转同步。原生画面需游戏内验收。");
        return changed;
    }
}
#endif
