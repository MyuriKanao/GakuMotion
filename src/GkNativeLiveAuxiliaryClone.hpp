#pragma once
namespace NativeLiveRuntime::auxiliary
{
    struct Pick { void* track{}; Rule rule; unsigned feature{}; };
    // Private copy of the admitted tracks of one source timeline. Source
    // TimelineAsset, tracks, clips, playable assets and profiles are not mutated.
    struct Clone
    {
        r::Root asset;
        std::vector<r::Root> owned;
        std::map<void*, void*> tracks;      // Action-scoped; cleared after binding.
        int rigTracks{}, environmentTracks{}, monitorTracks{}, audienceTracks{};

        void* Copy(void* source)
        {
            void* args[]{source}; void* value = r::Call(cloneObject, nullptr, args);
            owned.emplace_back(value);
            r::Require(value != source && r::Is(value, scriptableClass) && r::Alive(value), "auxiliary asset clone is shared");
            return value;
        }
        void* CopyProfile(void* source, r::Arena& roots)
        {
            // Profile mixers write component parameters; keep them private.
            r::Require(r::Is(source, bindings.profile), "auxiliary volume profile type mismatch");
            void* copy = Copy(source);
            void* originalList = r::FieldObject(source, bindings.profileComponents, roots);
            void* list = r::FieldObject(copy, bindings.profileComponents, roots);
            r::Require(list && originalList && list != originalList, "volume profile component list is shared");
            void* listClass = g_faceRuntime.objectClass(list);
            const auto countMethod = r::Find(listClass, "get_Count", false, {0x08}, {}, true);
            const auto itemMethod = r::Find(listClass, "get_Item", false, {0x12, bindings.volumeComponent}, {{0x08}}, true);
            const auto setter = r::Find(listClass, "set_Item", false, {0x01}, {{0x08}, {0x12, bindings.volumeComponent}}, true);
            r::Require(countMethod && itemMethod && setter, "private volume profile list access unavailable");
            const int count = r::Read<int>(countMethod, list);
            r::Require(count >= 0 && count <= 256, "volume profile component bound exceeded");
            for (int i = 0; i < count; ++i)
            {
                void* args[]{&i}; void* component = roots.Keep(r::Call(itemMethod, list, args));
                r::Require(r::Is(component, bindings.volumeComponent), "volume component type mismatch");
                void* privateComponent = Copy(component); void* setArgs[]{&i, privateComponent};
                r::Call(setter, list, setArgs);
            }
            return copy;
        }
        void IsolateClip(void* source, void* copy, r::Arena& roots)
        {
            void* klass = g_faceRuntime.objectClass(source);
            if (void* behaviour = r::ReferenceField(klass, "behaviour"))
            {
                void* original = r::FieldObject(source, behaviour, roots);
                void* privateValue = r::FieldObject(copy, behaviour, roots);
                r::Require(!original || (privateValue && privateValue != original), "auxiliary clip behaviour is shared");
            }
            if (void* field = r::Field(klass, "volumeProfile", {0x12, bindings.profile}))
                if (void* profile = r::FieldObject(copy, field, roots)) r::FieldObjectSet(copy, field, CopyProfile(profile, roots));
        }
        void* CopyTrack(void* source, const Rule& rule, r::Arena& roots)
        {
            const auto originals = r::Items(roots.Keep(r::Call(trackClips, source)), 8192);
            void* copy = Copy(source);
            const auto clips = r::Items(roots.Keep(r::Call(trackClips, copy)), 8192);
            r::Require(clips.size() == originals.size(), "auxiliary clip count changed during clone");
            // Verify isolation before any setter, as for equipment tracks.
            for (void* clip : clips)
                r::Require(clip && std::find(originals.begin(), originals.end(), clip) == originals.end(), "auxiliary TimelineClip is shared");
            r::FieldObjectSet(copy, trackParentField, nullptr);
            r::Call(clearChildren, copy); r::Call(clearMarkers, copy);
            r::Set(setMuted, copy, r::Read<bool>(mutedHierarchy, source));
            for (void* clip : clips)
            {
                void* original = roots.Keep(r::Call(clipAsset, clip));
                r::Require(r::Is(original, rule.clip), "unadmitted auxiliary clip");
                void* privateAsset = Copy(original); IsolateClip(original, privateAsset, roots);
                void* args[]{privateAsset}; r::Call(setClipAsset, clip, args);
                r::FieldObjectSet(clip, clipParentField, copy);
            }
            return copy;
        }
        bool Build(void* source, const std::vector<Pick>& picks)
        {
            r::Require(!asset && r::Is(source, timelineClass), "auxiliary source is not an unused timeline");
            if (picks.empty()) return false;
            r::Arena roots;
            // Publish the owned asset before copies so partial Build is cleanable.
            void* type = r::TypeObject(timelineClass, roots); void* args[]{type};
            asset = r::Root(r::Call(createAsset, nullptr, args));
            for (const auto& pick : picks)
            {
                r::Require(!tracks.contains(pick.track), "duplicate auxiliary flattened track");
                tracks.emplace(pick.track, CopyTrack(pick.track, pick.rule, roots));
                if (r::Read<bool>(mutedHierarchy, pick.track)) continue;
                pick.feature == plan::Rig ? ++rigTracks : pick.feature == plan::Environment ? ++environmentTracks :
                    pick.feature == plan::Monitor ? ++monitorTracks : ++audienceTracks;
            }
            for (const auto& pick : picks)
            {
                void* parent = roots.Keep(r::Call(trackParent, pick.track)); unsigned depth{};
                while (parent && parent != source && !tracks.contains(parent))
                {
                    r::Require(++depth <= 64 && r::Is(parent, groupClass), "unsupported auxiliary parent hierarchy");
                    parent = roots.Keep(r::Call(trackParent, parent));
                }
                void* addArgs[]{tracks.at(pick.track)};
                if (parent && tracks.contains(parent)) r::Call(addChild, tracks.at(parent), addArgs);
                else r::Call(addTrack, asset.Get(), addArgs);
            }
            return true;
        }
        void Clear()
        {
            tracks.clear();
            for (auto it = owned.rbegin(); it != owned.rend(); ++it) Destroy(*it);
            owned.clear(); Destroy(asset);
            rigTracks = environmentTracks = monitorTracks = audienceTracks = 0;
        }
    };
}
