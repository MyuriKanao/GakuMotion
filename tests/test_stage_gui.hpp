// Included after Harness; drives real ImGui widgets and the Core command queue.
void TestStageLightingGui(const std::filesystem::path& root)
{
    Harness app(root);
    app.core.configRoot = MotionPathToUtf8(app.fixture);
    app.core.gui.activePage = GuiPage::Stage;
    app.Settle();
    app.Click("Live 舞台灯光（实验）"); app.Settle();
    const auto* strength = app.ItemWithLabel("灯光强度");
    Check(strength && (strength->flags & ImGuiItemFlags_Disabled), "native lighting initially disables effect parameters");
    app.Click("灯光效果"); app.Settle();
    app.Click("青紫 / 彩色渐变"); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.mode == stage_light::Mode::Cycle,
        "choosing a real lighting combo item reaches the Core stage settings consumer");
    stage_load::Settings saved;
    std::string error;
    Check(saved.Load(MotionPathToUtf8(app.fixture / "stage.json"), error) && saved.lighting.mode == stage_light::Mode::Cycle,
        "GUI effect selection persists without changing the current stage");
    app.Click("灯光配色"); app.Settle();
    int presetItems = 0;
    for (const auto& palette : stage_light::palettes) if (app.ItemWithLabel(palette.name)) ++presetItems;
    Check(presetItems == static_cast<int>(stage_light::palettes.size()) && app.ItemWithLabel("自定义"),
        "palette combo lists every preset and the custom option");
    app.Click(stage_light::palettes[4].name); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.palette == 4 && !app.ItemWithLabel("颜色 1"),
        "a preset palette reaches Core and hides custom color pickers");
    app.Click("灯光配色"); app.Settle(); app.Click("自定义"); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.palette == stage_light::kCustomPalette &&
        app.ItemWithLabel("颜色 1") && app.ItemWithLabel("颜色 3") && !app.ItemWithLabel("颜色 4"),
        "custom palette shows one picker per configured color");
    Check(saved.Load(MotionPathToUtf8(app.fixture / "stage.json"), error) && saved.lighting.palette == stage_light::kCustomPalette,
        "custom palette choice persists");
    const auto customBefore = app.core.stageManager.settings.liveLighting.custom;
    app.Click("颜色 1"); app.Settle(); app.Key(ImGuiKey_Escape); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.custom == customBefore,
        "opening and closing a color picker without editing does not rewrite settings");
    app.core.gui.developerMode = true;
    app.core.gui.activePage = GuiPage::Developer;
    app.Settle();
    app.Click("摄影舞台替换"); app.Settle();
    app.Click("下次摄影背景"); app.Settle();
    int stageItems = 0;
    for (const auto& stage : stage_load::stages) if (app.ItemWithLabel(stage.name)) ++stageItems;
    Check(stageItems == static_cast<int>(stage_load::stages.size()), "stage dropdown renders the complete shared allowlist");
    app.Click("中庭舞台（傍晚）"); app.Settle();
    Check(app.core.stageManager.GetLoadReplacementKey() == "env_3d_live_courtyard-00-00-evening" &&
        saved.Load(MotionPathToUtf8(app.fixture / "stage.json"), error) && saved.lighting.mode == stage_light::Mode::Cycle,
        "new scene selection persists while retaining lighting preferences");
    app.core.gui.activePage = GuiPage::Stage;
    app.Settle();
    app.Click("光影效果"); app.Settle();
    const auto* mode = app.ItemWithLabel("灯光效果");
    Check(mode && (mode->flags & ImGuiItemFlags_Disabled), "global lighting switch disables automatic lighting controls");
}

void TestEmptyNativeLiveGui(const std::filesystem::path& root)
{
    Harness app(root);
    app.core.configRoot = MotionPathToUtf8(app.fixture);
    app.core.stageManager.settings.liveLighting.mode = stage_light::Mode::SongTimeline;
    app.core.gui.activePage = GuiPage::Stage;
    app.Settle();
    if (!app.ItemWithLabel("灯光效果")) { app.Click("Live 舞台灯光（实验）"); app.Settle(); }
    const auto* song = app.ItemWithLabel("原生灯光模板");
    const auto* refresh = app.ItemWithLabel("刷新原生模板");
    Check(song && !(song->flags & ImGuiItemFlags_Disabled), "empty template catalog still offers automatic current-stage matching");
    Check(refresh && !(refresh->flags & ImGuiItemFlags_Disabled), "catalog refresh stays available with an empty song list");
    Check(app.Text().find("自动（匹配当前舞台）") != std::string::npos, "empty catalog explains the next action instead of asking for an unavailable song");
}

void TestNativeLiveGui(const std::filesystem::path& root)
{
    native_live::catalog.Publish({{"music-gui-fixture@1", "Synthetic Agitato", "env_3d_gui_fixture", "tln_live_gui_fixture", 1, "TrueEnd"},
        {"music-gui-other@2", "Synthetic Other", "env_3d_gui_other", "tln_live_gui_other", 2}});
    Harness app(root);
    app.core.configRoot = MotionPathToUtf8(app.fixture);
    app.core.gui.activePage = GuiPage::Stage;
    const auto fixed = app.core.stageManager.settings.loadReplacementKey;
    app.Settle();
    if (!app.ItemWithLabel("灯光效果")) { app.Click("Live 舞台灯光（实验）"); app.Settle(); }
    app.Click("灯光效果"); app.Settle(); app.Click("原生模板 · 跟随曲库节拍"); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.mode == stage_light::Mode::SongTimeline,
        "native song mode reaches Core through real GUI command queue");
    const auto* bpm = app.ItemWithLabel("灯光 BPM");
    Check(bpm && (bpm->flags & ImGuiItemFlags_Disabled), "native song does not replace its BPM with procedural settings");
    app.Click("原生灯光模板"); app.Settle();
    app.Click("Synthetic Agitato / TrueEnd##music-gui-fixture@1"); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.songKey == "music-gui-fixture@1" &&
        app.core.stageManager.GetLoadReplacementKey() == "env_3d_gui_fixture", "song selection chooses its own paired stage");
    stage_load::Settings loaded; std::string error;
    Check(loaded.Load(MotionPathToUtf8(app.fixture / "stage.json"), error) && loaded.key == fixed &&
        loaded.lighting.songKey == "music-gui-fixture@1", "native preference persists without overwriting saved fixed stage");
    for (const char* label : {"原生环境渲染", "舞台大屏", "观众与荧光棒"})
    {
        const auto* layer = app.ItemWithLabel(label);
        Check(layer && !(layer->flags & ImGuiItemFlags_Disabled) && (layer->status & ImGuiItemStatusFlags_Checked),
            "native template exposes each enabled layer as an independent real checkbox");
        app.Click(label); app.Settle();
    }
    const auto& layers = app.core.stageManager.settings.liveLighting;
    Check(!layers.environment && !layers.monitor && !layers.audience,
        "native layer checkboxes reach the Core settings consumer");
    Check(loaded.Load(MotionPathToUtf8(app.fixture / "stage.json"), error) && loaded.lighting == layers && loaded.key == fixed,
        "native layer choices persist without changing the fixed stage preference");
    app.Click("原生灯光模板"); app.Settle();
    app.Click("Synthetic Other / 演出版本 2##music-gui-other@2"); app.Settle();
    Check(app.core.stageManager.GetLoadReplacementKey() == "env_3d_gui_other", "another song switches to another paired stage, not Agitato hardware");
    native_live::refreshRequested.store(false); app.Click("刷新原生模板"); app.Settle();
    Check(native_live::refreshRequested.exchange(false), "refresh requests main-thread catalog work");
    native_live::retryRequested.store(false); app.Click("重试原生灯光"); app.Settle();
    Check(native_live::retryRequested.exchange(false), "retry is explicit, not a per-frame load storm");
    const auto loadedKey=app.core.loadedMotionKey;
    app.Click("原生灯光模板");app.Settle();app.Click("自动（匹配当前舞台）");app.Settle();
    Check(app.core.stageManager.settings.liveLighting.songKey.empty() && app.core.loadedMotionKey==loadedKey,
        "automatic stage template matching never selects another library song");
    app.contextPatch=[](GuiContext& ctx){ctx.loadedMotionKey="ray-synthetic";ctx.libraryTempo={128,.25,1,"曲库元数据",""};ctx.tempoAnalyzing=false;};
    app.Settle();
    Check(app.Text().find("ray-synthetic")!=std::string::npos && app.Text().find("128.00")!=std::string::npos,
        "stage pane shows the actual library track and its BPM, separate from template selection");
    app.Click("灯光效果");app.Settle();app.Click("通用灯光 · 跟随曲库节拍");app.Settle();
    Check(app.core.stageManager.settings.liveLighting.mode==stage_light::Mode::AutoBeat && !app.ItemWithLabel("原生灯光模板"),
        "universal automatic lighting is available without any native-song template selector");
    Check(!app.ItemWithLabel("原生环境渲染") && !app.ItemWithLabel("舞台大屏") && !app.ItemWithLabel("观众与荧光棒"),
        "native composition layers are not advertised as procedural-mode capabilities");
    const auto* automaticBpm=app.ItemWithLabel("灯光 BPM");
    Check(automaticBpm && (automaticBpm->flags & ImGuiItemFlags_Disabled),"automatic mode does not use the global manual BPM slider");
}
