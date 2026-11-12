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
    const auto* song = app.ItemWithLabel("原生歌曲 / 版本");
    const auto* refresh = app.ItemWithLabel("刷新本地曲目");
    Check(song && (song->flags & ImGuiItemFlags_Disabled), "empty native catalog does not show an actionable empty song dropdown");
    Check(refresh && !(refresh->flags & ImGuiItemFlags_Disabled), "catalog refresh stays available with an empty song list");
    Check(app.Text().find("目录未就绪，请刷新本地曲目") != std::string::npos, "empty catalog explains the next action instead of asking for an unavailable song");
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
    app.Click("灯光效果"); app.Settle(); app.Click("原生歌曲时间轴"); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.mode == stage_light::Mode::SongTimeline,
        "native song mode reaches Core through real GUI command queue");
    const auto* bpm = app.ItemWithLabel("灯光 BPM");
    Check(bpm && (bpm->flags & ImGuiItemFlags_Disabled), "native song does not replace its BPM with procedural settings");
    app.Click("原生歌曲 / 版本"); app.Settle();
    app.Click("Synthetic Agitato / TrueEnd##music-gui-fixture@1"); app.Settle();
    Check(app.core.stageManager.settings.liveLighting.songKey == "music-gui-fixture@1" &&
        app.core.stageManager.GetLoadReplacementKey() == "env_3d_gui_fixture", "song selection chooses its own paired stage");
    stage_load::Settings loaded; std::string error;
    Check(loaded.Load(MotionPathToUtf8(app.fixture / "stage.json"), error) && loaded.key == fixed &&
        loaded.lighting.songKey == "music-gui-fixture@1", "native preference persists without overwriting saved fixed stage");
    app.Click("原生歌曲 / 版本"); app.Settle();
    app.Click("Synthetic Other / 演出版本 2##music-gui-other@2"); app.Settle();
    Check(app.core.stageManager.GetLoadReplacementKey() == "env_3d_gui_other", "another song switches to another paired stage, not Agitato hardware");
    native_live::refreshRequested.store(false); app.Click("刷新本地曲目"); app.Settle();
    Check(native_live::refreshRequested.exchange(false), "refresh requests main-thread catalog work");
    native_live::retryRequested.store(false); app.Click("重试原生灯光"); app.Settle();
    Check(native_live::retryRequested.exchange(false), "retry is explicit, not a per-frame load storm");
}
