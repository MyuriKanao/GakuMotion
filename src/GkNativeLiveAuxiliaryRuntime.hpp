#pragma once
namespace NativeLiveRuntime
{
    // Hosts admitted non-equipment tracks on their own source directors, so
    // exposed references (e.g. CampusVolumeTimelineAsset.volume) still resolve.
    // Each director receives a private TimelineAsset and a manual clock.
    struct AuxiliaryGraph
    {
        struct Session
        {
            r::Root director, original;
            auxiliary::Clone clone;
            bool assigned{}, stopped{};
        };
        std::vector<std::unique_ptr<Session>> sessions;
        std::vector<r::Root> lift; // Consumed by SourceComposition::Activate.
        int rigTracks{}, environmentTracks{}, monitorTracks{}, audienceTracks{}, skippedTracks{};
        double lastTime{-1};
        int lastFrame{INT_MIN};

        plan::Binding Classify(void* target, const SourceComposition& composition, unsigned& features, r::Arena& roots)
        {
            if (!target) return plan::Binding::None;
            void* go = r::Is(target, goClass) ? target : r::Is(target, StageLoader::componentClass) ?
                roots.Keep(r::Call(StageLoader::componentGo, target)) : nullptr;
            if (!go || r::Read<int>(StageLoader::goScene, go) != composition.scene) return plan::Binding::Foreign;
            features = composition.Features(r::Read<int>(StageLoader::instanceId, go));
            return features ? plan::Binding::Admitted : plan::Binding::Hidden;
        }
        std::vector<auxiliary::Pick> Decide(void* director, void* source, const SourceComposition& composition, r::Arena& roots)
        {
            auto& api = auxiliary::bindings;
            std::vector<auxiliary::Pick> picks;
            std::map<void*, unsigned> decided;
            for (void* track : r::Items(roots.Keep(r::Call(flattenedTracks, source)), 4096))
            {
                // Groups carry no clips; equipment/BPM tracks belong to NativeGraph.
                if (r::Is(track, groupClass) || r::Is(track, equipmentTrackClass) || r::Is(track, bpmClass)) continue;
                const auto rule = api.Match(track);
                plan::TrackFacts facts{rule.kind};
                if (rule.track)
                {
                    const auto clips = r::Items(roots.Keep(r::Call(trackClips, track)), 8192);
                    facts.clips = !clips.empty() && std::all_of(clips.begin(), clips.end(), [&](void* clip) {
                        return r::Is(roots.Keep(r::Call(clipAsset, clip)), rule.clip); });
                    if (rule.kind == plan::Track::Animation) facts.clipMode = r::Read<bool>(api.clipMode, track);
                    void* args[]{track};
                    facts.binding = Classify(roots.Keep(r::Call(api.getBinding, director, args)), composition, facts.rootFeatures, roots);
                    void* parent = roots.Keep(r::Call(trackParent, track));
                    if (parent && parent != source && !r::Is(parent, groupClass))
                    {
                        const auto found = decided.find(parent);
                        facts.nested = true; facts.parent = found == decided.end() ? 0u : found->second;
                    }
                }
                const unsigned feature = plan::Decide(facts, composition.mask);
                decided.emplace(track, feature);
                if (feature) picks.push_back({track, rule, feature}); else ++skippedTracks;
            }
            return picks;
        }
        void Bind(Session& session, r::Arena& roots)
        {
            auto& api = auxiliary::bindings;
            for (const auto& [original, copy] : session.clone.tracks)
            {
                void* args[]{original};
                void* target = roots.Keep(r::Call(api.getBinding, session.director.Get(), args));
                if (target) { void* bindArgs[]{copy, target}; r::Call(api.setBinding, session.director.Get(), bindArgs); }
            }
        }
        void Build(const SourceComposition& composition)
        {
            r::Require(sessions.empty() && lift.empty() && composition.captured && !composition.activated,
                "auxiliary source composition unavailable");
            r::Arena roots;
            for (void* director : Components(directorClass, composition.scene, roots))
            {
                void* go = roots.Keep(r::Call(StageLoader::componentGo, director));
                const int id = r::Read<int>(StageLoader::instanceId, go);
                const bool inPlace = composition.Features(id) != 0;
                if (!inPlace && !composition.Liftable(id)) continue;
                void* source = roots.Keep(r::Call(getAsset, director));
                if (!r::Is(source, timelineClass)) continue;
                // Equipment/BPM tracks never match a rule: NativeGraph's own
                // director drives them. The source equipment director may still
                // host that timeline's rig AnimationTracks.
                auto picks = Decide(director, source, composition, roots);
                if (picks.empty()) continue;
                // Publish before clone/binding mutations so partial Build remains cleanable.
                sessions.emplace_back(std::make_unique<Session>()); auto& session = *sessions.back();
                session.director = r::Root(director); session.original = r::Root(source);
                r::Require(session.clone.Build(source, picks), "auxiliary timeline produced no tracks");
                Bind(session, roots);
                r::Set(setAutoPlay, director, false); r::Set(setManual, director, manualMode); r::Set(setWrap, director, holdMode);
                session.assigned = true; void* assetArgs[]{session.clone.asset.Get()}; r::Call(setAsset, director, assetArgs);
                rigTracks += session.clone.rigTracks; environmentTracks += session.clone.environmentTracks;
                monitorTracks += session.clone.monitorTracks; audienceTracks += session.clone.audienceTracks;
                session.clone.tracks.clear(); // No raw managed pointer map survives Build.
                if (!inPlace) lift.emplace_back(director);
            }
        }
        // After the composition activates their GameObjects: build each graph
        // on the manual clock, as NativeGraph does for its own director.
        void Start()
        {
            for (auto& session : sessions)
            {
                r::Require(r::Alive(session->director.Get()), "auxiliary director expired");
                r::Call(directorPlay, session->director.Get()); session->stopped = false;
            }
        }
        bool Sample(double time)
        {
            r::Require(std::isfinite(time) && time >= 0, "auxiliary sample time is invalid");
            const int unityFrame = g_faceUnityFrame ? g_faceUnityFrame() : INT_MIN;
            if (unityFrame != INT_MIN && unityFrame == lastFrame && time == lastTime) return true;
            for (auto& session : sessions)
            {
                r::Require(r::Alive(session->director.Get()), "auxiliary director expired");
                // Manual evaluation only; the source Live is never played.
                r::Set(directorTime, session->director.Get(), time);
                r::Call(directorEvaluate, session->director.Get());
            }
            lastFrame = unityFrame; lastTime = time;
            return true;
        }
        bool Stop()
        {
            lift.clear();
            for (auto it = sessions.begin(); it != sessions.end();)
            {
                auto& session = **it;
                try
                {
                    if (r::Alive(session.director.Get()))
                    {
                        if (!session.stopped) { r::Call(directorStop, session.director.Get()); session.stopped = true; }
                        if (session.assigned)
                        { void* args[]{session.original.Get()}; r::Call(setAsset, session.director.Get(), args); session.assigned = false; }
                    }
                    session.clone.Clear(); it = sessions.erase(it);
                }
                catch (...) { ++it; }
            }
            if (!sessions.empty()) return false;
            rigTracks = environmentTracks = monitorTracks = audienceTracks = skippedTracks = 0;
            lastTime = -1; lastFrame = INT_MIN;
            return true;
        }
    };
}
