// Included after the synthetic native graph fixture helpers.
namespace
{
    void TestStagePlacement()
    {
        using namespace NativeLiveRuntime;
        for (int index = 0; index < 3; ++index)
        {
            const int stageScene = 200 + index * 2, sourceScene = stageScene + 1;
            auto stage = RootGo("Stage", stageScene); stage->position = {2, 4, 6};
            auto source = RootGo("Performer", sourceScene); source->position = {1, 3, 5}; source->active = false;
            Add(source, VirtualActorC);
            auto light = Child("Light", source); light->position = {0, 2, 0};
            auto camera = RootGo("PhotographyCamera", 190); camera->position = {10, -99, 14};
            const auto cameraPosition = World(camera);
            const StagePlacement::Vec3 photo{10.0f, -100.0f + index * 100.0f, 20.0f};
            StagePlacement placement; placement.Apply(stageScene, sourceScene, photo);
            Check(World(source) == photo, "each Live's performer anchor aligns to the Photography coordinate origin");
            Check(World(stage) == StagePlacement::Vec3{photo[0] + 1, photo[1] + 1, photo[2] + 1},
                "paired environment shares the source scene translation, including elevated or underground Photography origins");
            Check(World(light)[1] == photo[1] + 2 && light->position[1] == 2 && source->position[1] == 3,
                "light rig and local animation retain authored relative coordinates");
            Check(World(camera) == cameraPosition && !camera->parent,
                "Photography camera and its native controller are not moved or reparented");
            light->position[1] = 7;
            Check(World(light)[1] == photo[1] + 7, "absolute local animation samples remain in the aligned coordinate frame");
            Check(placement.Restore() && !stage->parent && !source->parent && !source->active && stage->active,
                "release restores environment parenting without reactivating a source actor or camera");
            Check(stage->position == StagePlacement::Vec3{2, 4, 6} && placement.Restore(),
                "restoration is exact and repeatable without cumulative translation");
        }
        auto stage = RootGo("Stage", 220), source = RootGo("Source", 221);
        StagePlacement unchanged; unchanged.Apply(220, 221, {});
        Check(!stage->parent && !source->parent && unchanged.children.empty() && unchanged.Restore(),
            "matching origins need no wrappers or hierarchy mutations");
        auto actor = Add(source, VirtualActorC); actor->name = "VirtualActor_00"; source->position = {0, 5, 0};
        auto secondary = RootGo("Secondary", 221); secondary->position = {0, 80, 0}; Add(secondary, VirtualActorC);
        StagePlacement primary; primary.Apply(220, 221, {0, 20, 0});
        Check(primary.offset[1] == 15 && primary.Restore(), "multiple source performers use the exact primary anchor, not search order");
        actor->name = "unknown";
        StagePlacement ambiguous;
        Check(Throws([&] { ambiguous.Apply(220, 221, {0, 20, 0}); }) && !stage->parent && ambiguous.Restore(),
            "ambiguous performer anchors fail before hierarchy changes");
        StagePlacement invalid;
        Check(Throws([&] { invalid.Apply(220, 222, {0, NAN, 0}); }) && !stage->parent && invalid.Restore(),
            "non-finite origins are rejected before changing the native stage");
    }
    void TestPlacementRecovery()
    {
        using namespace NativeLiveRuntime;
        auto stage = RootGo("Stage", 230); auto source = RootGo("Camera", 231); source->active = false;
        StagePlacement placement;
        failParent = 1;
        Check(Throws([&] { placement.Apply(230, 231, {0, -100, 0}); }) && !placement.children.empty(),
            "partial parent setter failure retains its restoration ledger");
        failParent = 1;
        Check(!placement.Restore() && placement.parents[0].go,
            "partial detach failure retains the wrapper and blocks completion");
        Check(placement.Restore() && !stage->parent && !placement.parents[0].go,
            "partial origin setup and cleanup can both be retried");
        placement.Apply(230, 231, {0, -100, 0});
        auto foreign = RootGo("Foreign", 232); auto expectedParent = stage->parent; stage->parent = foreign;
        Check(!placement.Restore() && stage->parent == foreign && !source->active,
            "external reparenting is not overwritten and source roots remain hidden during a failed restore");
        stage->parent = expectedParent;
        Check(placement.Restore() && !stage->parent, "restoration resumes once exact parenting is available");
        placement.Apply(230, 231, {0, -100, 0});
        auto wrapper = O(placement.parents[0].go.Get()); auto extra = Child("Untracked", wrapper);
        Check(!placement.Restore() && wrapper->alive && extra->alive,
            "untracked wrapper children are retained rather than destroyed with the wrapper");
        extra->parent = nullptr;
        Check(placement.Restore() && extra->alive, "wrapper cleanup can finish without deleting unrelated new objects");
    }
    void TestPlacementGraph()
    {
        using namespace NativeLiveRuntime;
        auto source = BuildLive(241); auto stage = RootGo("StageFloor", 240);
        auto actor = RootGo("VirtualActor_00", 241); actor->active = false; actor->position = {0, 4, 0}; Add(actor, VirtualActorC);
        NativeGraph graph; graph.Build(MakeRequest(240), 241, {}, StagePlacement::Vec3{0, -100, 0});
        Check(World(stage)[1] == -104 && World(source.realLight)[1] == -104 && !actor->active,
            "production graph aligns environment and source lighting before activating admitted roots");
        const auto position = World(source.realLight);
        ++frame; graph.Sample(15); ++frame; graph.Sample(2);
        Check(World(source.realLight) == position, "sampling and seeking do not accumulate stage placement offsets");
        Check(graph.Stop() && !stage->parent && !source.realLight->parent && !source.realLight->active && !actor->active,
            "graph stop removes origin wrappers and leaves every source root hidden");
        frame_hooks::Release();
    }
}
