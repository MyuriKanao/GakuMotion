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
namespace NativeLiveRuntime
{
    namespace live = GakuMotion::native_live;
    namespace light = GakuMotion::stage_light;
    struct Engine { live::Request request; SourceScene source; NativeGraph graph; bool built{}; };
    inline std::unique_ptr<Engine> engine;
    inline live::Session session;
    inline bool shuttingDown{};
    inline double requestedOffset{};

    inline std::optional<live::Request> Desired(const light::Settings& settings)
    {
        if (shuttingDown || settings.mode != light::Mode::SongTimeline || !ProtectionOnUnityThread() ||
            !g_runtimeReady.load(std::memory_order_acquire) || !g_activeSceneVerified) return {};
        const auto song = live::catalog.Find(settings.songKey);
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
            { engine->graph.Build(engine->request, engine->source.scene); engine->built = true; error.clear(); }
            return live::Operation::Ready;
        }
        catch (const std::exception& problem) { error = problem.what(); return live::Operation::Failed; }
    }
    inline bool Sample(double time)
    {
        try { if (engine) engine->graph.offset = requestedOffset; return engine && engine->built && engine->graph.Sample(time); }
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
        if (!error.empty()) return "原生灯光：" + error + "（未回退为程序化灯光）";
        if (session.phase == live::Phase::Releasing) return "原生灯光：正在恢复舞台并释放自有时间轴场景";
        if (session.phase == live::Phase::Loading) return "原生灯光：正在读取本地歌曲时间轴";
        if (engine && session.phase == live::Phase::Ready)
            return "原生歌曲灯光：" + engine->request.song.title + "；设备轨 " +
                std::to_string(engine->graph.clone.equipmentTracks) + " / BPM 轨 " +
                std::to_string(engine->graph.clone.bpmTracks) + "（外部动作与镜头不接管；画面待验收）";
        if (!live::catalog.Find(settings.songKey)) return "请选择本地原生歌曲 / 演出版本；目录未就绪时点击刷新";
        return "原生灯光待机：请重新进入摄影以载入所选歌曲的配套舞台，然后播放外部动作";
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
            return StageLightingRuntime::Update(settings, seconds, notice);
        }
        requestedOffset = settings.nativeOffset;
        const bool ok = session.Update(Desired(settings), seconds);
        if (engine) engine->graph.offset = settings.nativeOffset;
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
