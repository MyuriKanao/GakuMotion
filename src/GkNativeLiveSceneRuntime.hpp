#pragma once
#include "GkNativeLiveTaskRuntime.hpp"
namespace NativeLiveRuntime
{
    inline void* NewComponent(void* go, void* klass, r::Arena& roots)
    {
        void* type = r::TypeObject(klass, roots); void* args[]{type};
        void* value = roots.Keep(r::Call(addComponent, go, args));
        r::Require(r::Is(value, klass) && r::Alive(value), "owned component creation failed"); return value;
    }
    inline void NewGo(r::Root& owned, const char* name, int scene, r::Arena& roots)
    {
        // Publish ownership before .ctor or MoveGameObjectToScene: a partial
        // initialization failure still leaves a helper that cleanup can see.
        owned = r::Root(r::objectNew(goClass)); void* go = owned.Get();
        void* text = r::String(name, roots); void* args[]{text}; r::Call(goCtor, go, args);
        bool active = false; r::Set(StageLoader::setActive, go, active);
        void* moveArgs[]{go, &scene}; r::Call(moveScene, nullptr, moveArgs);
        r::Require(r::Read<int>(StageLoader::goScene, go) == scene, "owned helper scene mismatch");
    }
    inline void Destroy(r::Root& root)
    {
        if (!root) return;
        void* value = root.Get();
        if (r::Alive(value)) { void* args[]{value}; r::Call(destroyObject, nullptr, args); }
        root.Reset();
    }
    struct SourceScene
    {
        enum class Step { Empty, Bundle, Scene, Ready, UnloadScene, UnloadBundle, Done };
        r::Root helper, loader, cancellation;
        Task task;
        Step step{Step::Empty};
        std::set<int> beforeScenes;
        std::map<int, bool> originalRootStates;
        std::string bundle, path, sceneName;
        int scene{}, ownerScene{}, stageScene{};
        bool canceled{}, unloadSceneStarted{}, unloadBundleStarted{}, tokenReady{};

        std::set<int> Scenes()
        {
            const int count = r::Read<int>(sceneCount);
            r::Require(count >= 0 && count <= 4096, "loaded scene count unavailable");
            std::set<int> result;
            for (int index = 0; index < count; ++index)
            { void* args[]{&index}; result.insert(r::Read<int>(sceneAt, nullptr, args)); }
            return result;
        }
        void InvokeTask(const void* method, void* text = nullptr, bool none = false)
        {
            r::Arena roots;
            void* token = roots.Keep(r::Call(none ? noneToken : ctsToken, none ? nullptr : cancellation.Get()));
            void* data = token ? g_safeBones.unbox(token) : nullptr;
            r::Require(data != nullptr, "owned cancellation token unavailable");
            void* args[]{text, data}; void* tokenArgs[]{data};
            task.Start(method, loader.Get(), text ? args : tokenArgs);
        }
        void Begin(const GakuMotion::native_live::Request& request)
        {
            r::Require(step == Step::Empty, "previous native source has not been released");
            RequireCached(request.song.timeline); RequireCached(request.song.stage);
            step = Step::Bundle; // Partial initialization is also releasable.
            beforeScenes = Scenes(); ownerScene = request.owner.sceneHandle; stageScene = request.stageScene;
            bundle = BundleKey(request.song.timeline);
            r::Arena roots;
            NewGo(helper, "GakuMotion.NativeLive.Source.Plugin", ownerScene, roots);
            loader = r::Root(NewComponent(helper.Get(), sceneLoaderClass, roots));
            cancellation = r::Root(r::objectNew(ctsClass)); r::Call(ctsCtor, cancellation.Get()); tokenReady = true;
            void* text = r::String(bundle, roots);
            InvokeTask(sceneLoadBundle, text);
        }
        void SelectScenePath()
        {
            r::Arena roots;
            void* octoLoader = r::FieldObject(loader.Get(), loaderField, roots);
            void* assetOperator = r::FieldObject(octoLoader, operatorField, roots);
            r::Require(r::Is(assetOperator, operatorClass), "native source is not a scene bundle");
            void* paths = roots.Keep(r::Call(scenePaths, assetOperator));
            const auto items = r::Items(paths, 16);
            r::Require(items.size() == 1, "native source scene is ambiguous");
            path = r::Text(items.front(), 1024);
            const auto name = GakuMotion::native_live::SceneNameFromPath(path);
            if (!name) throw r::Failure("native source scene path unusable: \"" + path + "\"");
            sceneName = *name;
            // A scene already loaded by the game is never adopted or unloaded.
            for (int handle : Scenes())
            {
                void* args[]{&handle};
                r::Require(r::Text(roots.Keep(r::Call(StageLoader::sceneName, nullptr, args)), 1024) != sceneName,
                    "native source scene is already in use by the game");
            }
            beforeScenes = Scenes();
        }
        std::vector<void*> Roots(r::Arena& roots)
        { void* array = roots.Keep(r::Call(sceneRoots, loader.Get())); return array ? r::Items(array, 4096) : std::vector<void*>{}; }
        void ObserveOwnedScene()
        {
            int current{}; StageLightingRuntime::fieldGet(loader.Get(), loaderSceneField, &current);
            if (!current) return;
            r::Require(!beforeScenes.contains(current) && current != ownerScene && current != stageScene,
                "native source loader refers to a preexisting scene");
            r::Arena roots; void* args[]{&current};
            r::Require(r::Text(roots.Keep(r::Call(StageLoader::sceneName, nullptr, args)), 1024) == sceneName,
                "native source loader scene name mismatch");
            scene = current;
        }
        void Isolate()
        {
            r::Arena roots; const auto objects = Roots(roots);
            r::Require(!objects.empty(), "native source has no roots");
            std::set<int> found;
            for (void* go : objects)
            {
                r::Require(r::Alive(go), "native source root expired");
                const int handle = r::Read<int>(StageLoader::goScene, go);
                r::Require(handle && !beforeScenes.contains(handle) && handle != ownerScene && handle != stageScene,
                    "native source does not own a new scene");
                found.insert(handle);
            }
            r::Require(found.size() == 1, "native source roots span multiple scenes");
            scene = *found.begin();
            void* nameArgs[]{&scene};
            r::Require(r::Text(roots.Keep(r::Call(StageLoader::sceneName, nullptr, nameArgs)), 1024) == sceneName, "owned source scene name mismatch");
            // No source actor/camera/audio track is played by this plugin.
            for (void* go : objects)
            {
                const int id = r::Read<int>(StageLoader::instanceId, go);
                if (!originalRootStates.contains(id))
                    originalRootStates.emplace(id, r::Read<bool>(StageLoader::activeSelf, go));
                r::Set(StageLoader::setActive, go, false);
            }
        }
        GakuMotion::native_live::Operation Poll()
        {
            using GakuMotion::native_live::Operation;
            const auto result = task.Poll();
            if (result != Operation::Ready) return result;
            if (step == Step::Bundle)
            {
                SelectScenePath(); r::Arena roots; void* text = r::String(sceneName, roots);
                media_gate::Arm(sceneName, beforeScenes);
                step = Step::Scene; InvokeTask(sceneLoad, text); return Operation::Pending;
            }
            if (step == Step::Scene) { Isolate(); step = Step::Ready; }
            return step == Step::Ready ? Operation::Ready : Operation::Failed;
        }
        bool Release()
        {
            using GakuMotion::native_live::Operation;
            if (step == Step::Empty) return true;
            if (!canceled)
            {
                if (cancellation && tokenReady) r::Call(ctsCancel, cancellation.Get());
                if (loader) r::Call(sceneCancel, loader.Get());
                canceled = true;
            }
            const auto completion = task.Poll();
            if (completion == Operation::Pending) return false;
            if (completion == Operation::Failed && step == Step::UnloadScene)
            { unloadSceneStarted = false; step = Step::Scene; }
            if (completion == Operation::Failed && step == Step::UnloadBundle) unloadBundleStarted = false;
            if (scene)
            {
                void* args[]{&scene};
                if (!r::Read<bool>(StageLoader::sceneLoaded, nullptr, args))
                { scene = 0; unloadSceneStarted = true; step = Step::UnloadScene; }
            }
            if ((step == Step::Scene || step == Step::Ready) && !unloadSceneStarted)
            {
                // A canceled load can finish successfully. The loader is ours,
                // but validate its roots again before asking it to unload.
                r::Arena roots;
                ObserveOwnedScene();
                const auto objects = Roots(roots);
                if (!objects.empty() || scene)
                {
                    if (!objects.empty()) Isolate();
                    InvokeTask(sceneUnload, nullptr, true); unloadSceneStarted = true; step = Step::UnloadScene; return false;
                }
            }
            if (scene && unloadSceneStarted)
            {
                void* args[]{&scene};
                r::Require(!r::Read<bool>(StageLoader::sceneLoaded, nullptr, args), "owned scene unload incomplete");
                scene = 0;
            }
            if (loader && !unloadBundleStarted)
            {
                InvokeTask(bundleUnload, nullptr, true); unloadBundleStarted = true; step = Step::UnloadBundle; return false;
            }
            if (cancellation && tokenReady) r::Call(ctsDispose, cancellation.Get());
            cancellation.Reset(); loader.Reset(); Destroy(helper);
            media_gate::Disarm();
            step = Step::Done;
            return true;
        }
    };
}
