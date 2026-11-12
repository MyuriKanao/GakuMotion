// Production cloner, composition, auxiliary tracks, identity-scoped hooks and
// driver; synthetic Unity boundary. Layout follows the static root census.
#include "native_live_graph_fixture.hpp"
#include "GkNativeLiveClonerRuntime.hpp"
#include "GkNativeLiveHooksRuntime.hpp"
#include "GkNativeLiveAuxiliaryClone.hpp"
#include "GkNativeLiveCompositionRuntime.hpp"
#include "GkNativeLiveAuxiliaryRuntime.hpp"
#include "GkNativeLiveGraphRuntime.hpp"
#include <algorithm>
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
    Object* Child(const char* name, Object* parent)
    { NativeLiveRuntime::r::Arena roots; auto go = O(NativeLiveRuntime::NewGo(name, parent->scene, roots)); go->parent = parent; go->active = true; return go; }
    Object* RootGo(const char* name, int scene)
    { NativeLiveRuntime::r::Arena roots; auto go = O(NativeLiveRuntime::NewGo(name, scene, roots)); go->active = true; return go; }
    Object* Add(Object* go, Kind kind) { NativeLiveRuntime::r::Arena roots; return O(NativeLiveRuntime::NewComponent(go, C(kind), roots)); }
    void TestCloner()
    {
        auto source = Source(); const auto tracks = source->items;
        auto originalLight = O(tracks[3]); auto originalClip = O(originalLight->items[0]); auto originalAsset = originalClip->asset;
        auto originalChildren = originalLight->children;
        NativeLiveRuntime::TimelineClone clone; std::vector<std::pair<void*, void*>> pairs; clone.Build(source, &pairs);
        Check(clone.equipmentTracks == 2 && clone.bpmTracks == 1 && clone.clips == 3, "native equipment and BPM tracks cloned, unrelated track excluded");
        Check(pairs.size() == 3 && std::all_of(pairs.begin(), pairs.end(), [](auto& p) { return p.first != p.second; }),
            "original-to-copy pairs are reported for binding copy");
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
    GakuMotion::native_live::Request MakeRequest(int stage)
    {
        return {{"music-fixture@1", "Synthetic", "env_3d_fixture", "tln_live_fixture", 1},
            {true, 0x123, 17, true, GakuMotion::injection::OwnerRole::Photography, false}, stage};
    }
    void TestPairedStageDriver()
    {
        using namespace NativeLiveRuntime;
        r::Arena roots;
        auto sourceAsset = Source();
        auto sourceGo = RootGo("EquipmentController", 33); sourceGo->active = false; // As left by SourceScene::Isolate.
        auto sourceDirector = Add(sourceGo, Director); sourceDirector->asset = sourceAsset;
        auto sourceController = Add(sourceGo, Equipment); sourceController->director = sourceDirector;
        auto stageGo = O(NewGo("paired", 22, roots)); auto receiver = Add(stageGo, Receiver);
        auto request = MakeRequest(22);
        calls.clear(); NativeGraph graph; graph.Build(request, 33, {{sourceGo->id, true}});
        Check(!graph.sourceReceiver && graph.receiver.Get() == receiver, "paired stage receiver is preferred when present");
        Check(graph.helper && graph.equipment && captures > 0, "native driver owns helpers and captures stage before binding");
        Check(std::find(calls.begin(), calls.end(), "capture") < std::find(calls.begin(), calls.end(), "bind"), "snapshot precedes native receiver mutation");
        Check(O(graph.helper.Get())->scene == 17 && O(graph.director.Get())->manual == manualMode, "driver is owned by Photography and uses manual clock");
        Check(!sourceDirector->graph && !sourceController->receiver && !sourceController->enabled, "source Live controller/director never played or rebound");
        Check(!sourceGo->active, "with a paired stage receiver the source rig root stays hidden");
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
        // Two receivers on the stage remain ambiguous; never pick one.
        Add(stageGo, Receiver);
        NativeGraph ambiguous;
        Check(Throws([&] { ambiguous.Build(request, 33); }), "ambiguous paired stage receivers rejected");
        ambiguous.Stop();
    }
    struct LiveScene
    {
        Object *receiverRoot, *receiver, *equipmentRoot, *director, *controller, *eqValue, *realLight, *lightAnchor;
        Object *environment, *envDirector, *envAnimator, *monitor, *monitorDirector, *monitorCamera, *video, *rt;
        Object *crowd, *crowdDirector, *crowdVolume, *master, *lightDirector, *actor, *cameraRoot, *audio, *audioSource, *particles;
        Object *equipmentTimeline;
    };
    LiveScene BuildLive(int scene)
    {
        LiveScene s{};
        s.receiverRoot = RootGo("EquipmentReceiver_all-fixture", scene); s.receiver = Add(s.receiverRoot, Receiver);
        Add(Child("Spot", s.receiverRoot), LightC);
        s.equipmentRoot = RootGo("EquipmentController", scene);
        s.director = Add(s.equipmentRoot, Director); s.controller = Add(s.equipmentRoot, Equipment); s.controller->director = s.director;
        auto valueGo = Child("Value", s.equipmentRoot); s.eqValue = Add(valueGo, EqValue);
        s.equipmentTimeline = Source(); s.director->asset = s.equipmentTimeline;
        s.director->bindings[s.equipmentTimeline->items[3]] = s.eqValue;
        auto rigAnim = AuxTrack(AnimTrack, s.equipmentTimeline, AnimClip); s.director->bindings[rigAnim] = valueGo;
        s.realLight = RootGo("RealLight", scene); s.lightAnchor = Add(Child("Anchor", s.realLight), LightC);
        s.environment = RootGo("Environment", scene); s.envDirector = Add(s.environment, Director);
        s.envAnimator = Child("Mesh", s.environment); Add(s.envAnimator, Renderer);
        auto envTimeline = New(Asset); s.envDirector->asset = envTimeline;
        auto envAnim = AuxTrack(AnimTrack, envTimeline, AnimClip); s.envDirector->bindings[envAnim] = s.envAnimator;
        auto infinite = AuxTrack(AnimTrack, envTimeline, AnimClip); infinite->clipMode = false; s.envDirector->bindings[infinite] = s.envAnimator;
        s.monitor = RootGo("Monitor", scene); s.monitorDirector = Add(s.monitor, Director);
        s.rt = New(Texture);
        s.monitorCamera = Add(Child("RTCamera", s.monitor), CameraC); s.monitorCamera->asset = s.rt;
        auto canvas = Add(Child("Canvas", s.monitor), CanvasC); canvas->mode = 1;
        Add(Child("Screen", s.monitor), MonitorC); s.video = Add(Child("Video", s.monitor), VideoC); s.video->mode = 2;
        auto monTimeline = New(Asset); s.monitorDirector->asset = monTimeline;
        auto videoTrack = AuxTrack(VideoTrack, monTimeline); s.monitorDirector->bindings[videoTrack] = s.video;
        s.crowd = RootGo("Crowd", scene); s.crowdDirector = Add(s.crowd, Director); s.crowdVolume = Add(s.crowd, CrowdC);
        auto crowdTimeline = New(Asset); s.crowdDirector->asset = crowdTimeline;
        auto crowdTrack = AuxTrack(CrowdTrack, crowdTimeline); s.crowdDirector->bindings[crowdTrack] = s.crowdVolume;
        s.master = RootGo("MasterTimeline", scene); Add(s.master, Director);
        auto lightGo = Child("Light", s.master); s.lightDirector = Add(lightGo, Director);
        auto lightTimeline = New(Asset); s.lightDirector->asset = lightTimeline;
        auto anchorTrack = AuxTrack(LightTrack, lightTimeline); s.lightDirector->bindings[anchorTrack] = s.lightAnchor;
        auto volTrack = AuxTrack(VolTrack, lightTimeline); (void)volTrack;
        auto actorTrack = AuxTrack(OtherTrack, lightTimeline); s.lightDirector->bindings[actorTrack] = s.lightAnchor;
        s.actor = Add(Child("Actor", s.master), ActorC);
        auto cameraGo = Child("Camera", s.master); s.lightDirector->bindings[AuxTrack(AnimTrack, lightTimeline, AnimClip)] = cameraGo;
        s.cameraRoot = RootGo("CameraRoot", scene); Add(s.cameraRoot, CameraC);
        s.audio = RootGo("AudioGroup", scene); s.audioSource = Add(s.audio, AudioC);
        s.particles = RootGo("ParticleSystem", scene);
        return s;
    }
    void TestSourceComposition()
    {
        using namespace NativeLiveRuntime;
        const int scene = 44, stage = 23;
        auto s = BuildLive(scene);
        std::map<int, bool> original;
        for (auto* root : {s.receiverRoot, s.equipmentRoot, s.realLight, s.environment, s.monitor, s.crowd, s.master, s.cameraRoot, s.audio, s.particles})
        { original.emplace(root->id, true); root->active = false; }
        original[s.particles->id] = true;
        auto stageGo = RootGo("env_3d_fixture", stage); Add(stageGo, LightC);
        globals[77] = New(Texture); auto priorGlobal = globals[77];
        calls.clear(); captures = restores = 0;
        NativeGraph graph; auto request = MakeRequest(stage);
        graph.Build(request, scene, original);
        Check(graph.sourceReceiver && graph.receiver.Get() == s.receiver, "timeline-scene receiver drives the rig when the stage has none");
        Check(s.receiverRoot->active && s.realLight->active && s.environment->active && s.monitor->active && s.crowd->active,
            "rig, environment, screen and audience roots are shown from the owned source");
        Check(!s.master->active && !s.cameraRoot->active && !s.audio->active,
            "Live controller, cameras and audio stay hidden");
        Check(s.equipmentRoot->active && !s.controller->enabled && s.director->manual == manualMode &&
            s.director->asset != s.equipmentTimeline,
            "equipment value targets are shown; source controller disabled and its original timeline never played");
        Check(!s.particles->active, "stand-alone particle root without its control track stays hidden");
        Check(!s.audioSource->enabled && !s.controller->enabled, "source audio and equipment controller are disabled");
        Check(std::count(calls.begin(), calls.end(), std::string("audio-stop")) == 1, "source audio is stopped once");
        Check(s.video->mode == 0, "screen video audio output is silenced");
        Check(O(graph.director.Get())->bindings.size() == 1, "EquipmentValue binding copied onto the owned equipment director");
        const auto bound = O(graph.director.Get())->bindings.begin()->second;
        Check(bound == s.eqValue, "copied binding targets the authored EquipmentValue");
        Check(graph.auxiliary.sessions.size() == 5, "equipment-rig, environment, screen, crowd and lifted light directors each host a private timeline");
        Check(s.director->asset != s.equipmentTimeline && O(s.director->asset)->items.size() == 1,
            "source equipment director hosts only its rig animation; equipment tracks stay with the owned driver");
        Check(O(s.lightDirector->go)->parent == nullptr && O(s.lightDirector->go)->active, "bare child light director is lifted out of the hidden controller");
        Check(graph.auxiliary.rigTracks == 2 && graph.auxiliary.environmentTracks == 2 && graph.auxiliary.monitorTracks == 1 &&
            graph.auxiliary.audienceTracks == 1, "admitted track counts per layer");
        Check(graph.auxiliary.skippedTracks == 4, "infinite clip, unsupported, actor-control and camera-bound tracks are skipped");
        Check(s.envDirector->asset != nullptr && O(s.envDirector->asset)->items.size() == 1, "private environment timeline holds only clip-mode animation");
        Check(s.monitorDirector->graph && s.crowdDirector->graph && s.envDirector->graph, "hosted source directors build graphs after activation");
        Check(captures == 1, "only the game's stage is snapshotted");
        calls.clear(); ++frame; graph.Sample(5);
        const auto firstEval = std::find(calls.begin(), calls.end(), "evaluate");
        const auto receiverCall = std::find(calls.begin(), calls.end(), "receiver");
        Check(std::count(calls.begin(), calls.end(), std::string("evaluate")) == 6 && firstEval < receiverCall,
            "auxiliary directors evaluate before the native equipment cycle");
        Check(s.monitorDirector->time == 5 && s.crowdDirector->time == 5, "auxiliary directors share the absolute timeline");
        calls.clear(); graph.Sample(5);
        Check(calls.empty(), "same-frame duplicate sample is skipped for auxiliary directors too");
        globals[77] = s.rt;
        Check(graph.Stop(), "composition release completes");
        Check(globals[77] == priorGlobal, "screen shader global restored to its prior texture");
        Check(!s.receiverRoot->active && !s.monitor->active && !s.crowd->active && !O(s.lightDirector->go)->active,
            "every source root, including lifted directors, is hidden on release");
        Check(s.envDirector->asset && O(s.envDirector->asset)->items.size() == 2 && s.director->asset == s.equipmentTimeline,
            "hosted source directors get their original timelines back");
        Check(!graph.composition.scene && graph.auxiliary.sessions.empty(), "composition and auxiliary ledgers are empty");
        frame_hooks::Release();
    }
    void TestLayerSwitches()
    {
        using namespace NativeLiveRuntime;
        const int scene = 55, stage = 24;
        auto s = BuildLive(scene);
        for (auto* root : {s.receiverRoot, s.equipmentRoot, s.realLight, s.environment, s.monitor, s.crowd, s.master, s.cameraRoot, s.audio, s.particles})
            root->active = false;
        auto request = MakeRequest(stage); request.monitor = false; request.audience = false;
        NativeGraph graph; graph.Build(request, scene);
        Check(s.receiverRoot->active && s.environment->active, "light rig and environment stay on");
        Check(!s.monitor->active && !s.crowd->active, "disabled screen and audience layers keep their roots hidden");
        Check(graph.auxiliary.monitorTracks == 0 && graph.auxiliary.audienceTracks == 0 && graph.auxiliary.sessions.size() == 3,
            "disabled layers clone no tracks and host no directors");
        Check(s.video->mode == 2, "hidden screen video player is left untouched");
        Check(graph.Stop(), "partial-layer composition releases");
        frame_hooks::Release();
        auto t = BuildLive(66); t.environment->active = false;
        auto off = MakeRequest(25); off.environment = off.monitor = off.audience = false;
        NativeGraph rigOnly; rigOnly.Build(off, 66);
        Check(t.receiverRoot->active && t.realLight->active && !t.environment->active, "all optional layers off keeps only the rig");
        Check(rigOnly.Stop(), "rig-only composition releases");
        frame_hooks::Release();
        auto u = BuildLive(77); u.receiverRoot->active = false; Add(Child("StrayActor", u.receiverRoot), ActorC);
        NativeGraph unsafe;
        Check(Throws([&] { unsafe.Build(MakeRequest(26), 77); }) && !u.receiverRoot->active,
            "receiver root mixed with actor content fails without showing it");
        Check(unsafe.Stop(), "failed build remains releasable");
        frame_hooks::Release();
        auto w = BuildLive(88); w.receiverRoot->active = false;
        NativeGraph hiddenReceiver;
        Check(Throws([&] { hiddenReceiver.Build(MakeRequest(27), 88, {{w.receiverRoot->id, false}}); }),
            "authored-inactive receiver root is reported instead of driving a hidden rig");
        Check(hiddenReceiver.Stop(), "inactive-receiver failure remains releasable");
        frame_hooks::Release();
    }
}
int main()
{
    InitializeMethods(); TestCloner(); TestPairedStageDriver(); TestSourceComposition(); TestLayerSwitches();
    std::printf("Native Live graph adapter: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
