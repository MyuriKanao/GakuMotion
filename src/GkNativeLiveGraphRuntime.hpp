#pragma once
namespace NativeLiveRuntime
{
    struct NativeGraph
    {
        r::Root helper, director, equipment, receiver;
        TimelineClone clone;
        NativeSnapshot snapshot;
        ComponentId receiverIdentity;
        double duration{}, offset{}, lastTime{-1};
        int lastFrame{INT_MIN};
        bool touchedReceiver{}, stopped{}, disposed{}, hidden{};

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
            duration = r::Read<double>(directorDuration, matches.front());
            r::Require(std::isfinite(duration) && duration > 0 && duration <= 86400, "native song duration unavailable");
            return roots.Keep(r::Call(getAsset, matches.front()));
        }
        void Build(const GakuMotion::native_live::Request& request, int sourceScene)
        {
            r::Arena roots;
            const auto receivers = Components(receiverClass, request.stageScene, roots);
            r::Require(receivers.size() == 1, "paired native stage receiver is not unique");
            receiver = r::Root(receivers.front()); receiverIdentity = Identity(receiver.Get(), roots);
            for (void* controller : Components(equipmentClass, request.stageScene, roots))
                r::Require(r::Call(equipmentReceiver, controller) != receiver.Get(), "paired stage receiver is already driven by the game");
            clone.Build(SelectTimeline(sourceScene, roots));
            r::Set(setFixedDuration, clone.asset.Get(), duration); r::Set(setDurationMode, clone.asset.Get(), fixedMode);
            snapshot.Capture(request.stageScene);
            r::Require(frame_hooks::Install(), "native equipment frame hook lease unavailable");
            NewGo(helper, "GakuMotion.NativeLive.Driver.Plugin", request.owner.sceneHandle, roots);
            director = r::Root(NewComponent(helper.Get(), directorClass, roots));
            r::Set(setAutoPlay, director.Get(), false); r::Set(setManual, director.Get(), manualMode); r::Set(setWrap, director.Get(), holdMode);
            void* assetArgs[]{clone.asset.Get()}; r::Call(setAsset, director.Get(), assetArgs);
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
                "native light driver or paired receiver expired");
            r::Require(r::Read<bool>(validGraph, equipment.Get()) && frame_hooks::IsOwned(equipment.Get(),
                frame_hooks::controllerId.load(), frame_hooks::controllerScene.load()), "native driver graph or owner identity changed");
            r::Arena roots;
            r::Require(Identity(receiver.Get(), roots) == receiverIdentity, "native stage receiver identity changed");
            const double time = GakuMotion::native_live::SampleTime(seconds, offset, duration);
            const int currentFrame = g_faceUnityFrame ? g_faceUnityFrame() : INT_MIN;
            if (currentFrame != INT_MIN && currentFrame == lastFrame && time == lastTime) return true;
            // Both auto controller callbacks and this receiver's auto Update
            // are suppressed only for the owned identities. One native job
            // cycle is evaluated at the external player's absolute time.
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
            if (!snapshot.Restore()) return false;
            frame_hooks::controllerId.store(0); frame_hooks::receiverId.store(0);
            equipment.Reset(); director.Reset(); receiver.Reset(); Destroy(helper); clone.Clear();
            return true;
        }
    };
}
