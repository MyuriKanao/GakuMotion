#pragma once
// All Unity operations in this module run on host main-thread callbacks.
// Reuses the client's native equipment engine; never runs the source Live.
#include "GkNativeLiveReflection.hpp"
#include "GkNativeLiveBindings.hpp"
#include "GkNativeLiveCatalogRuntime.hpp"
#include "GkNativeLiveSceneRuntime.hpp"
#include "GkNativeLiveSnapshotRuntime.hpp"
#include "GkNativeLiveClonerRuntime.hpp"
#include "GkNativeLiveHooksRuntime.hpp"
#include "GkNativeLiveGraphRuntime.hpp"
#include "GkNativeLiveTempoRuntime.hpp"
namespace NativeLiveRuntime
{
    namespace live = GakuMotion::native_live;
    namespace light = GakuMotion::stage_light;
    struct Engine { live::Request request; SourceScene source; NativeGraph graph; GakuMotion::tempo::BeatMap beats; bool built{}; };
    inline std::unique_ptr<Engine> engine;
    inline live::Session session;
    inline bool shuttingDown{};
    inline double requestedOffset{};

    inline std::optional<live::Request> Desired(const light::Settings& settings)
    {
        if (shuttingDown || settings.mode != light::Mode::SongTimeline || !ProtectionOnUnityThread() ||
            !g_runtimeReady.load(std::memory_order_acquire) || !g_activeSceneVerified) return {};
        if (!GakuMotion::Core::Instance().LibraryTempoReady()) return {};
        const auto song = settings.songKey.empty()?live::catalog.ForStage(SceneOwnerRuntime::cachedActiveScene):live::catalog.Find(settings.songKey);
        if (!song || SceneOwnerRuntime::cachedActiveScene != song->stage || !g_verifiedActiveSceneHandle ||
            GakuMotion::stage_load::receipt.Context(song->stage, g_verifiedOwnerSceneName, g_sceneOwner) !=
                GakuMotion::injection::Context::Photography) return {};
        return live::Request{*song, g_sceneOwner, g_verifiedActiveSceneHandle};
    }
    inline bool Begin(const live::Request& request)
    {
        try
        {
            r::Require(metadataReady && !engine, metadataReady ? "previous native source is held" : error.c_str());
            r::Require(StageLightingRuntime::lightReady && StageLightingRuntime::beamReady, "native Light/VLB restoration interface unavailable");
            r::Require(StageLightingRuntime::Restore(), "procedural lighting restoration pending");
            engine = std::make_unique<Engine>(); engine->request = request; engine->source.Begin(request); return true;
        }
        catch (const std::exception& problem) { error = problem.what(); return false; }
    }
    inline live::Operation PollLoad()
    {
        try
        {
            r::Require(engine != nullptr, "native source is missing");
            const auto result = engine->source.Poll();
            if (result != live::Operation::Ready)
            { if (result == live::Operation::Failed) error = "native source load faulted or canceled"; return result; }
            if (!engine->built)
            {
                engine->graph.Build(engine->request, engine->source.scene);
                BuildTemplateBeats(engine->graph,engine->beats);engine->built = true; error.clear();
            }
            return live::Operation::Ready;
        }
        catch (const std::exception& problem) { error = problem.what(); return live::Operation::Failed; }
    }
    inline bool Sample(double time)
    {
        try
        {
            auto& core=GakuMotion::Core::Instance();
            if (!engine || !engine->built || !core.LibraryTempoReady()) return false;
            const double beat=core.libraryTempo.value.Beat(core.LightingAudioTime(time)+requestedOffset);
            engine->graph.offset=0;
            return engine->graph.Sample(engine->beats.Seconds(beat));
        }
        catch (const std::exception& problem) { error = problem.what(); return false; }
    }
    inline bool Release()
    {
        if (!engine) return true;
        if (!ProtectionOnUnityThread()) return false;
        try
        {
            if (!engine->graph.Stop() || !engine->source.Release()) return false;
            engine.reset(); return true;
        }
        catch (const std::exception& problem) { error = std::string("原生灯光清理待重试：") + problem.what(); return false; }
    }
    inline bool Restore()
    {
        if (!session.Release()) return false;
        return StageLightingRuntime::Restore();
    }
    inline std::string Notice(const light::Settings& settings)
    {
        if (!metadataReady) return "原生灯光接口未匹配：" + error;
        const auto& core=GakuMotion::Core::Instance();
        if (core.libraryTempo.analyzing) return "正在分析曲库当前曲目的 BPM / 拍点";
        if (!core.LibraryTempoReady()) return "曲库节拍待就绪："+core.libraryTempo.value.error;
        if (!error.empty()) return "原生灯光：" + error + "（未回退为程序化灯光）";
        if (session.phase == live::Phase::Releasing) return "原生灯光：正在恢复舞台并释放自有时间轴场景";
        if (session.phase == live::Phase::Loading) return "原生灯光：正在读取本地歌曲时间轴";
        if (engine && session.phase == live::Phase::Ready)
            return "曲库节拍驱动："+core.loadedMotionKey+"；原生模板：" + engine->request.song.title + "；设备轨 " +
                std::to_string(engine->graph.clone.equipmentTracks) + " / BPM 轨 " +
                std::to_string(engine->graph.clone.bpmTracks) + "（外部动作与镜头不接管；画面待验收）";
        if (settings.songKey.empty()) return "自动匹配当前舞台的原生灯光模板；无模板时可选择通用节拍灯光（不伪装为原生编排）";
        if (!live::catalog.Find(settings.songKey)) return "原生灯光模板目录待就绪；播放曲目仍由曲库选择";
        return "原生模板待机：请重新进入摄影载入模板配套舞台；灯光将按曲库当前曲目的拍数重定时";
    }
    inline void PollRestoration(bool allowed, const light::Settings& settings)
    {
        if (!ProtectionOnUnityThread()) return;
        PollCatalog();
        if (live::retryRequested.exchange(false)) { session.Retry(); if (metadataReady) error.clear(); }
        if (session.Held())
        {
            const auto desired = allowed ? Desired(settings) : std::nullopt;
            if (!desired || !engine || !engine->request.Same(*desired) || session.phase == live::Phase::Releasing) session.Release();
        }
        if (!session.Held()) StageLightingRuntime::PollRestoration(allowed && settings.mode != light::Mode::SongTimeline);
    }
    inline bool Update(const light::Settings& settings, double seconds, std::string& notice)
    {
        if (!ProtectionOnUnityThread()) return false;
        if (settings.mode != light::Mode::SongTimeline)
        {
            if (!session.Release()) { notice = Notice(settings); return false; }
            if (settings.mode==light::Mode::AutoBeat)
            {
                auto& core=GakuMotion::Core::Instance();auto driven=settings;
                if (!core.LibraryTempoReady())
                {
                    driven.mode=light::Mode::Native;StageLightingRuntime::Update(driven,seconds,notice);
                    notice=core.libraryTempo.analyzing?"正在分析当前曲目节拍":"曲库节拍待就绪："+core.libraryTempo.value.error;return false;
                }
                driven.mode=light::Mode::Cycle;driven.bpm=60;driven.speed=1;
                const double time=core.libraryTempo.value.Beat(core.LightingAudioTime(seconds)+settings.nativeOffset);
                const bool ok=StageLightingRuntime::Update(driven,time,notice);
                notice="曲库节拍："+core.loadedMotionKey+" / "+std::to_string(core.libraryTempo.value.bpm)+" BPM；"+notice;
                return ok;
            }
            return StageLightingRuntime::Update(settings, seconds, notice);
        }
        requestedOffset = settings.nativeOffset;
        const bool ok = session.Update(Desired(settings), seconds);
        notice = Notice(settings); return ok;
    }
    inline void Initialize(HMODULE module)
    {
        Resolve(module); session.api = {Begin, PollLoad, Sample, Release}; shuttingDown = false;
    }
    inline void Shutdown()
    {
        shuttingDown = true;
        const bool clean = Restore();
        // Keep handles if asynchronous Unity cleanup is still pending. Do not
        // perform off-thread Unity calls or unload another scene as a shortcut.
        if (!clean) GakuMotion::host::Warn("[GakuMotion][NativeLive] shutdown cleanup pending: %s", error.c_str());
        if (clean) frame_hooks::Release();
    }
}
