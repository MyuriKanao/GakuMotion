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
