#pragma once
#ifdef GKMOTION_GUI_ACTIVE
namespace GakuMotion
{
    inline void DrawLibraryTempo(const GuiContext& ctx)
    {
        ImGui::TextWrapped("当前播放曲目：%s（在曲库选择；此处不切换音频或动作）",ctx.loadedMotionKey.empty()?"未加载":ctx.loadedMotionKey.c_str());
        if (ctx.tempoAnalyzing) ImGui::TextWrapped("正在分析当前曲目的 BPM / 首拍位置……");
        else if (ctx.libraryTempo.Ready()) ImGui::TextWrapped("源音频 BPM：%.2f  ·  %s  ·  置信度 %.0f%%",ctx.libraryTempo.bpm,ctx.libraryTempo.source.c_str(),ctx.libraryTempo.confidence*100);
        else ImGui::TextWrapped("节拍待就绪：%s。可在曲目 info.json 设置 bpm 与 beat_offset_seconds，随后重新加载当前曲目。",ctx.libraryTempo.error.empty()?"请在曲库加载曲目或等待分析":ctx.libraryTempo.error.c_str());
    }
    inline bool DrawNativeLiveSong(stage_light::Settings& lighting, const GuiContext& ctx)
    {
        bool changed = false;
        const auto songs = native_live::catalog.Snapshot();
        const auto selected = native_live::catalog.Find(lighting.songKey);
        auto labelOf = [](const native_live::Song& song) {
            return song.title + " / " + (song.variant.empty() ? "演出版本 " + std::to_string(song.grade) : song.variant);
        };
        DrawLibraryTempo(ctx);
        const auto caption = selected ? labelOf(*selected) : lighting.songKey.empty()?"自动（匹配当前舞台）":"已保存模板未就绪";
        if (ImGui::BeginCombo("原生灯光模板", caption.c_str()))
        {
            if (ImGui::Selectable("自动（匹配当前舞台）",lighting.songKey.empty())) { lighting.songKey.clear();changed=true; }
            for (const auto& song : songs)
            {
                const auto label = labelOf(song) + "##" + song.key;
                if (ImGui::Selectable(label.c_str(), song.key == lighting.songKey))
                { lighting.songKey = song.key; changed = true; }
                if (song.key == lighting.songKey) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        changed |= ImGui::SliderFloat("灯光拍点偏移", &lighting.nativeOffset, -10.0f, 10.0f, "%.2f s");
        changed |= ImGui::Checkbox("原生环境渲染", &lighting.environment);
        changed |= ImGui::Checkbox("舞台大屏", &lighting.monitor);
        changed |= ImGui::Checkbox("观众与荧光棒", &lighting.audience);
        ImGui::TextWrapped("分层选择仅作用于原生模板；修改后重建自有舞台会话。环境渲染包含演出包的后期 Volume、网格与贴花，大屏包含屏幕相机与内嵌视频，观众包含群众与荧光棒。演员、镜头、音频、CRI 背景视频及独立粒子特效不接管；不自动补下载缺失视频。实际状态以运行提示为准。");
        if (ImGui::Button("刷新原生模板")) native_live::refreshRequested.store(true);
        ImGui::SameLine();
        if (ImGui::Button("重试原生灯光")) native_live::retryRequested.store(true);
        if (selected)
        {
            ImGui::TextWrapped("配套舞台：%s", selected->stage.c_str());
            ImGui::TextWrapped("原生时间轴：%s", selected->timeline.c_str());
            // The paired stage is loaded automatically; it is intentionally not
            // in the fixed stage list.
            ImGui::TextWrapped("配套舞台由模板自动载入，不在“下次摄影背景”列表中。选好模板后退出偶像摄影再进入；"
                "进入后若用游戏原生功能切换背景或横竖屏，会回到原生摄影场景，需要再次重新进入。");
        }
        if (!ctx.nativeLiveCatalogNotice.empty()) ImGui::TextWrapped("%s", ctx.nativeLiveCatalogNotice.c_str());
        ImGui::TextWrapped("曲库负责选歌；这里仅选灯控模板。默认自动匹配当前舞台，灯光按当前曲目的 BPM / 拍点重映射，不是按模板原曲秒数播放。手动换模板时需重新进入摄影载入配套舞台。");
        ImGui::TextWrapped("原生模板保留其灯具编排，节拍适配不等于为外部歌曲复原专属 MV 编排。无模板可用时请选择通用节拍模式；不自动伪装降级。");
        return changed;
    }
}
#endif
