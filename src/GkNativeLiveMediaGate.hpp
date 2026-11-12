#pragma once
#include "GkNativeLiveMediaPolicy.hpp"
namespace NativeLiveRuntime::media_gate
{
    using Callback = void (*)(void*, const void*);
    inline Callback original{};
    inline void* lease{};
    inline bool ready{};
    inline std::string expectedName;
    inline std::set<int> preexisting;
    inline unsigned blocked{};

    inline bool Matches(void* value)
    {
        if (!ProtectionOnUnityThread() || expectedName.empty()) return false;
        r::Arena roots;
        void* go = roots.Keep(r::Call(StageLoader::componentGo, value));
        const int scene = r::Read<int>(StageLoader::goScene, go);
        if (!scene || preexisting.contains(scene)) return false;
        void* args[]{const_cast<int*>(&scene)};
        const auto name = r::Text(roots.Keep(r::Call(StageLoader::sceneName, nullptr, args)), 1024);
        return GakuMotion::native_live::IsOwnedMediaScene(scene, name, expectedName, preexisting);
    }
    inline void OnEnable(void* value, const void* method)
    {
        // This callback precedes SourceScene::Isolate. Suppress the movie's
        // auto-download/setup only in the new, explicitly requested source scene.
        try { if (Matches(value)) { ++blocked; return; } }
        catch (...) { /* Unproven identities retain normal game behavior. */ }
        if (original) original(value, method);
    }
    inline void Arm(const std::string& name, const std::set<int>& before)
    {
        r::Require(ProtectionOnUnityThread() && expectedName.empty(), "previous media source gate is held");
        if (!lease)
        {
            void* klass = r::Class("campus-submodule.Runtime.dll", "Campus.Timeline", "CampusTimelineCriVideoPlayer");
            const void* method = r::Find(klass, "OnEnable", false, {0x01}, {});
            r::Require(method && GakuMotion::host::hooks.acquireBatch, "source movie lifecycle guard unavailable");
            void* address = *static_cast<void* const*>(method);
            r::Require(address != nullptr, "source movie lifecycle address unavailable");
            const GakuHookRequest request{address, reinterpret_cast<void*>(OnEnable), reinterpret_cast<void**>(&original)};
            const auto status = GakuMotion::host::hooks.acquireBatch(GakuMotion::host::hooks.context, 1, &request);
            if (status == GAKU_OK || original) lease = address;
            ready = status == GAKU_OK && original;
        }
        r::Require(ready, "source movie lifecycle hook lease unavailable");
        expectedName = name; preexisting = before; blocked = 0;
    }
    inline void Disarm() { expectedName.clear(); preexisting.clear(); }
    inline void Release()
    {
        if (!expectedName.empty()) return; // Retain the guard while owned unloading is pending.
        if (lease && GakuMotion::host::hooks.release &&
            GakuMotion::host::hooks.release(GakuMotion::host::hooks.context, lease) == GAKU_OK) { lease = nullptr; ready = false; }
    }
}
