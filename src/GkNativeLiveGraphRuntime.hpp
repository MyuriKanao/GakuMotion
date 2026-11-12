#pragma once
namespace NativeLiveRuntime
{
    namespace plan = GakuMotion::native_live::composition;
    struct NativeGraph
    {
        r::Root helper, director, equipment, receiver, sourceDirector;
        TimelineClone clone;
        NativeSnapshot snapshot;
        SourceComposition composition;
        AuxiliaryGraph auxiliary;
        ComponentId receiverIdentity;
        double duration{}, offset{}, lastTime{-1};
        int lastFrame{INT_MIN}, equipmentBindings{};
        bool touchedReceiver{}, stopped{}, disposed{}, hidden{}, sourceReceiver{};

        void* SelectTimeline(int sourceScene, r::Arena& roots)
        {
            std::set<void*> candidates;
            for (void* controller : Components(equipmentClass, sourceScene, roots))
            {
                void* candidate = r::FieldObject(controller, equipmentDirectorField, roots);
                if (r::Is(candidate, directorClass) && r::Alive(candidate)) candidates.insert(candidate);
            }
            if (candidates.empty())
                for (void* candidate : Components(directorClass, sourceScene, roots)) candidates.insert(candidate);
            std::vector<void*> matches;
            for (void* candidate : candidates)
            {
                void* asset = roots.Keep(r::Call(getAsset, candidate));
                if (!r::Is(asset, timelineClass)) continue;
                for (void* track : r::Items(roots.Keep(r::Call(flattenedTracks, asset)), 4096))
                    if (r::Is(track, equipmentTrackClass)) { matches.push_back(candidate); break; }
            }
            r::Require(matches.size() == 1, "native equipment timeline director is ambiguous or absent");
            sourceDirector = r::Root(matches.front());
            duration = r::Read<double>(directorDuration, matches.front());
            r::Require(std::isfinite(duration) && duration > 0 && duration <= 86400, "native song duration unavailable");
            return roots.Keep(r::Call(getAsset, matches.front()));
        }
        // Live packages carry the receiver and light rig in the timeline scene;
        // older paired stages carry it themselves. Exactly one source is used.
        void* SelectReceiver(const GakuMotion::native_live::Request& request, int sourceScene,
            const std::map<int, bool>& sourceRoots, r::Arena& roots)
        {
            const auto paired = Components(receiverClass, request.stageScene, roots);
            r::Require(paired.size() <= 1, "paired native stage receiver is ambiguous");
            sourceReceiver = paired.empty();
            unsigned layers{};
            if (request.environment) layers |= plan::Environment;
            if (request.monitor) layers |= plan::Monitor;
            if (request.audience) layers |= plan::Audience;
            composition.Capture(sourceScene, sourceRoots, layers, sourceReceiver);
            if (sourceReceiver) return composition.Receiver();
            for (void* controller : Components(equipmentClass, request.stageScene, roots))
                r::Require(r::Call(equipmentReceiver, controller) != paired.front(), "paired stage receiver is already driven by the game");
            return paired.front();
        }
        // Authored per-track EquipmentValue objects (equipment id, target) stay
        // in the owned source scene. Read their bindings before the auxiliary
        // graph reassigns the source director's asset; targets stay pinned.
        std::vector<std::pair<void*, void*>> EquipmentTargets(const std::vector<std::pair<void*, void*>>& pairs,
            int sourceScene, r::Arena& roots)
        {
            auto& api = auxiliary::bindings;
            std::vector<std::pair<void*, void*>> result;
            for (const auto& [original, copy] : pairs)
            {
                void* args[]{original};
                void* target = roots.Keep(r::Call(api.getBinding, sourceDirector.Get(), args));
                if (!target) continue;
                void* go = r::Is(target, goClass) ? target : r::Is(target, StageLoader::componentClass) ?
                    roots.Keep(r::Call(StageLoader::componentGo, target)) : nullptr;
                if (go && r::Read<int>(StageLoader::goScene, go) == sourceScene) result.emplace_back(copy, target);
            }
            return result;
        }
        void Build(const GakuMotion::native_live::Request& request, int sourceScene, const std::map<int, bool>& sourceRoots = {})
        {
            r::Arena roots;
            receiver = r::Root(SelectReceiver(request, sourceScene, sourceRoots, roots));
            receiverIdentity = Identity(receiver.Get(), roots);
            std::vector<std::pair<void*, void*>> pairs;
            clone.Build(SelectTimeline(sourceScene, roots), &pairs);
            r::Set(setFixedDuration, clone.asset.Get(), duration); r::Set(setDurationMode, clone.asset.Get(), fixedMode);
            // EquipmentValue targets are only active when the source rig is shown.
            const auto targets = sourceReceiver ? EquipmentTargets(pairs, sourceScene, roots) : decltype(pairs){};
            // Only the game's stage needs a restoration snapshot; the owned
            // source scene is hidden and unloaded on release.
            snapshot.Capture(request.stageScene);
            auxiliary.Build(composition);
            composition.Activate(auxiliary.lift);
            auxiliary.lift.clear();
            auxiliary.Start();
            r::Require(frame_hooks::Install(), "native equipment frame hook lease unavailable");
            NewGo(helper, "GakuMotion.NativeLive.Driver.Plugin", request.owner.sceneHandle, roots);
            director = r::Root(NewComponent(helper.Get(), directorClass, roots));
            r::Set(setAutoPlay, director.Get(), false); r::Set(setManual, director.Get(), manualMode); r::Set(setWrap, director.Get(), holdMode);
            void* assetArgs[]{clone.asset.Get()}; r::Call(setAsset, director.Get(), assetArgs);
            for (const auto& [copy, target] : targets)
            {
                void* bindArgs[]{copy, target};
                r::Call(auxiliary::bindings.setBinding, director.Get(), bindArgs); ++equipmentBindings;
            }
            equipment = r::Root(NewComponent(helper.Get(), equipmentClass, roots));
            frame_hooks::controllerId.store(r::Read<int>(StageLoader::instanceId, equipment.Get()));
            frame_hooks::controllerScene.store(request.owner.sceneHandle);
            frame_hooks::receiverId.store(receiverIdentity.component); frame_hooks::receiverScene.store(receiverIdentity.scene);
            r::FieldObjectSet(equipment.Get(), equipmentDirectorField, director.Get());
            touchedReceiver = true; // Record before a potentially partial mutation.
            r::FieldObjectSet(equipment.Get(), equipmentReceiverField, receiver.Get());
            r::Set(StageLoader::setActive, helper.Get(), true);
            r::Call(equipmentInit, equipment.Get());
            void* bindArgs[]{receiver.Get()}; r::Call(equipmentBind, equipment.Get(), bindArgs);
            r::Call(directorPlay, director.Get());
            r::Require(r::Read<bool>(validGraph, equipment.Get()), "native equipment playable graph did not become valid");
        }
        bool Sample(double seconds)
        {
            r::Require(director && equipment && receiver && r::Alive(director.Get()) && r::Alive(equipment.Get()) && r::Alive(receiver.Get()),
                "native light driver or receiver expired");
            r::Require(r::Read<bool>(validGraph, equipment.Get()) && frame_hooks::IsOwned(equipment.Get(),
                frame_hooks::controllerId.load(), frame_hooks::controllerScene.load()), "native driver graph or owner identity changed");
            r::Arena roots;
            r::Require(Identity(receiver.Get(), roots) == receiverIdentity, "native receiver identity changed");
            const double time = GakuMotion::native_live::SampleTime(seconds, offset, duration);
            const int currentFrame = g_faceUnityFrame ? g_faceUnityFrame() : INT_MIN;
            if (currentFrame != INT_MIN && currentFrame == lastFrame && time == lastTime) return true;
            // Environment, screen and audience tracks first, so equipment jobs
            // see this frame's anchors and volumes. Then one native equipment
            // cycle; the owned receiver and controller callbacks are suppressed.
            auxiliary.Sample(time);
            frame_hooks::originalReceiver(receiver.Get(), receiverUpdate);
            r::Set(directorTime, director.Get(), time); r::Call(directorEvaluate, director.Get());
            frame_hooks::originalUpdate(equipment.Get(), equipmentUpdate);
            frame_hooks::originalLate(equipment.Get(), equipmentLate);
            lastFrame = currentFrame; lastTime = time;
            return true;
        }
        bool Stop()
        {
            if (director && !stopped)
            { if (r::Alive(director.Get())) r::Call(directorStop, director.Get()); stopped = true; }
            if (helper && !hidden)
            { if (r::Alive(helper.Get())) r::Set(StageLoader::setActive, helper.Get(), false); hidden = true; }
            if (equipment && !disposed)
            { if (r::Alive(equipment.Get())) r::Call(equipmentDispose, equipment.Get()); disposed = true; }
            if (receiver && touchedReceiver)
            {
                if (r::Alive(receiver.Get())) r::Call(receiverClear, receiver.Get());
                touchedReceiver = false;
            }
            // Hide the source roots first (no active Volume keeps a private
            // profile that is about to be destroyed), then stop the hosted
            // directors, then restore the stage so no source rig overwrites it.
            if (!composition.Hide() || !auxiliary.Stop()) return false;
            if (!snapshot.Restore()) return false;
            frame_hooks::controllerId.store(0); frame_hooks::receiverId.store(0);
            if (!composition.Release()) return false;
            equipment.Reset(); director.Reset(); receiver.Reset(); sourceDirector.Reset(); Destroy(helper); clone.Clear();
            equipmentBindings = 0;
            return true;
        }
    };
}
