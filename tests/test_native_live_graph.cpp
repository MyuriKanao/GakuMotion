// Production cloner, identity-scoped hooks and driver; synthetic Unity boundary.
#include "native_live_graph_fixture.hpp"
#include "GkNativeLiveClonerRuntime.hpp"
#include "GkNativeLiveHooksRuntime.hpp"
#include "GkNativeLiveGraphRuntime.hpp"
using namespace NativeFixture;
namespace
{
    int passed{}, failed{};
    void Check(bool ok, const char* message)
    { if (ok) ++passed; else { ++failed; std::printf("FAIL: %s\n", message); } }
    template<class F> bool Throws(F action) { try { action(); return false; } catch (const std::exception&) { return true; } }
    Object* Source()
    {
        auto source = New(Asset);
        auto group = New(Group); group->parent = source;
        auto light = Track(EquipmentTrack, group, true);
        auto nested = Track(EquipmentTrack, light);
        light->children.push_back(nested);
        source->items = {Track(BpmTrack, source), Track(OtherTrack, source), group, light, nested};
        return source;
    }
    void TestCloner()
    {
        auto source = Source(); const auto tracks = source->items;
        auto originalLight = O(tracks[3]); auto originalClip = O(originalLight->items[0]); auto originalAsset = originalClip->asset;
        auto originalChildren = originalLight->children;
        NativeLiveRuntime::TimelineClone clone; clone.Build(source);
        Check(clone.equipmentTracks == 2 && clone.bpmTracks == 1 && clone.clips == 3, "native equipment and BPM tracks cloned, unrelated track excluded");
        Check(source->items == tracks && originalLight->children == originalChildren && originalClip->asset == originalAsset && originalClip->parent == originalLight,
            "source timeline, child list, clip and parent references remain unchanged");
        auto output = O(clone.asset.Get());
        Check(output != source && output->items.size() == 2, "only admitted root tracks attached to owned timeline");
        Object* copiedLight = nullptr;
        for (void* track : output->items) if (O(track)->kind == EquipmentTrack) copiedLight = O(track);
        Check(copiedLight && copiedLight != originalLight && copiedLight->children.size() == 1, "equipment subtrack hierarchy preserved");
        auto copiedClip = copiedLight ? O(copiedLight->items[0]) : nullptr;
        Check(copiedClip && copiedClip != originalClip && copiedClip->asset != originalAsset && copiedClip->parent == copiedLight,
            "managed TimelineClip and mutable playable asset are isolated copies");
        Check(copiedClip && !copiedClip->asset->clipController && copiedClip->asset->clipTrack == copiedLight,
            "cloned clip never retains original controller or track");
        Check(clone.copies.empty(), "action-scoped raw instance map discarded");
        clone.Clear();
        Check(!clone.asset && originalLight->alive && originalAsset->alive && source->alive, "cleanup destroys only owned assets");
        shallowClone = true; const int before = mutations;
        NativeLiveRuntime::TimelineClone shared;
        Check(Throws([&] { shared.Build(source); }) && mutations == before, "shared managed clip clone rejected before any setters");
        shared.Clear(); shallowClone = false;
        auto noLights = New(Asset); noLights->items = {Track(OtherTrack, noLights)};
        NativeLiveRuntime::TimelineClone empty;
        Check(Throws([&] { empty.Build(noLights); }), "zero light tracks never reports native ready"); empty.Clear();
        auto missingBpm = New(Asset); missingBpm->items = {Track(EquipmentTrack, missingBpm, true)};
        NativeLiveRuntime::TimelineClone noBpm;
        Check(Throws([&] { noBpm.Build(missingBpm); }), "BPM-driven light clips require a native BPM track"); noBpm.Clear();
        auto invalid = Source(); O(O(invalid->items[3])->items[0])->asset = New(OtherClip);
        NativeLiveRuntime::TimelineClone invalidClone;
        Check(Throws([&] { invalidClone.Build(invalid); }), "actor/audio/unknown clip on light track rejected"); invalidClone.Clear();
        auto nestedControl = Source(); O(nestedControl->items[2])->kind = OtherTrack;
        NativeLiveRuntime::TimelineClone hierarchy;
        Check(Throws([&] { hierarchy.Build(nestedControl); }), "unsupported control hierarchy is not silently flattened"); hierarchy.Clear();
        auto duplicate = Source(); duplicate->items.push_back(duplicate->items[3]);
        NativeLiveRuntime::TimelineClone duplicateClone;
        Check(Throws([&] { duplicateClone.Build(duplicate); }), "duplicate flattened source track rejected"); duplicateClone.Clear();
    }
    void TestDriver()
    {
        using namespace NativeLiveRuntime;
        r::Arena roots;
        auto sourceAsset = Source();
        auto sourceGo = O(NewGo("source", 33, roots));
        auto sourceDirector = O(NewComponent(sourceGo, directorClass, roots)); sourceDirector->asset = sourceAsset;
        auto sourceController = O(NewComponent(sourceGo, equipmentClass, roots)); sourceController->director = sourceDirector;
        auto stageGo = O(NewGo("paired", 22, roots)); auto receiver = O(NewComponent(stageGo, receiverClass, roots));
        GakuMotion::native_live::Request request{{"music-fixture@1", "Synthetic", "env_3d_fixture", "tln_live_fixture", 1},
            {true, 0x123, 17, true, GakuMotion::injection::OwnerRole::Photography, false}, 22};
        calls.clear(); NativeGraph graph; graph.Build(request, 33);
        Check(graph.helper && graph.equipment && captures > 0, "native driver owns helpers and captures stage before binding");
        Check(std::find(calls.begin(), calls.end(), "capture") < std::find(calls.begin(), calls.end(), "bind"), "snapshot precedes native receiver mutation");
        Check(O(graph.helper.Get())->scene == 17 && O(graph.director.Get())->manual == manualMode, "driver is owned by Photography and uses manual clock");
        Check(!sourceDirector->graph && !sourceController->receiver, "source Live controller/director never played or rebound");
        calls.clear(); graph.offset = 2; graph.Sample(10);
        Check(calls == std::vector<std::string>{"receiver", "time", "evaluate", "update", "late"}, "exactly one complete native frame follows manual timeline evaluation");
        Check(O(graph.director.Get())->time == 12, "native driver applies external time plus offset");
        calls.clear(); graph.Sample(10);
        Check(calls.empty(), "duplicate actor callback in same frame does not double-schedule native jobs");
        ++frame; graph.Sample(10);
        Check(O(graph.director.Get())->time == 12, "paused external clock freezes native timeline across frames");
        ++frame; graph.Sample(1);
        Check(O(graph.director.Get())->time == 3, "backward seek samples absolute native timeline");
        ++frame; graph.Sample(100);
        Check(O(graph.director.Get())->time == 60, "native timeline clamps rather than loops");
        O(graph.director.Get())->graph = false;
        Check(Throws([&] { graph.Sample(12); }), "disposed native graph rejected before equipment jobs run");
        O(graph.director.Get())->graph = true;
        calls.clear();
        frame_hooks::Update(graph.equipment.Get(), equipmentUpdate); frame_hooks::Late(graph.equipment.Get(), equipmentLate);
        frame_hooks::Receiver(receiver, receiverUpdate);
        Check(calls.empty(), "automatic native callbacks suppressed only for leased component identities");
        frame_hooks::Update(sourceController, equipmentUpdate); frame_hooks::Late(sourceController, equipmentLate);
        Check(calls == std::vector<std::string>{"update", "late"}, "unrelated game native controllers always forward");
        calls.clear(); mainThread = false; frame_hooks::Update(graph.equipment.Get(), equipmentUpdate); mainThread = true;
        Check(calls == std::vector<std::string>{"update"}, "off-thread callback performs no Unity ownership query");
        auto helper = O(graph.helper.Get()); auto asset = O(graph.clone.asset.Get());
        restoreReady = false;
        Check(!graph.Stop() && graph.helper && !helper->active && !O(graph.director.Get())->graph, "failed restoration retains owned roots but synchronously stops/hides driver");
        Check(std::find(calls.begin(), calls.end(), "clear") < std::find(calls.begin(), calls.end(), "restore"), "native material properties cleared before original snapshot restored");
        restoreReady = true;
        Check(graph.Stop() && !graph.helper && !asset->alive && !helper->alive && receiver->alive && sourceAsset->alive,
            "restoration retry releases only own helper/timeline, retains paired stage and source assets");
        frame_hooks::Release();
        NativeGraph wrongStage; request.stageScene = 99;
        Check(Throws([&] { wrongStage.Build(request, 33); }), "stage without paired receiver rejected, no generic light fallback");
    }
}
int main()
{
    InitializeMethods(); TestCloner(); TestDriver();
    std::printf("Native Live graph adapter: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
