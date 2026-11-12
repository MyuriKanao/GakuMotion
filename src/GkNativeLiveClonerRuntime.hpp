#pragma once
namespace NativeLiveRuntime
{
    struct TimelineClone
    {
        r::Root asset;
        std::vector<r::Root> ownedAssets;
        int equipmentTracks{}, bpmTracks{}, clips{};
        bool needsBpm{};
        std::map<void*, void*> copies; // Action-scoped only; cleared after Build.

        bool Allowed(void* track) const { return r::Is(track, equipmentTrackClass) || r::Is(track, bpmClass); }
        void* KeepClone(void* source)
        {
            void* args[]{source}; void* copy = r::Call(cloneObject, nullptr, args);
            ownedAssets.emplace_back(copy);
            r::Require(copy != source && r::Is(copy, scriptableClass) && r::Alive(copy), "native asset was not cloned");
            return copy;
        }
        void* CopyTrack(void* source, r::Arena& roots)
        {
            const bool equipment = r::Is(source, equipmentTrackClass);
            const auto originals = r::Items(roots.Keep(r::Call(trackClips, source)), 8192);
            void* copy = KeepClone(source);
            const auto cloned = r::Items(roots.Keep(r::Call(trackClips, copy)), 8192);
            r::Require(cloned.size() == originals.size(), "native clip list clone mismatch");
            // Verify isolation BEFORE ANY setter. TimelineClip.parentTrack's
            // setter removes from the old track, so use the exact reference
            // field on our new managed clip instead of touching the source.
            for (void* clip : cloned)
                r::Require(clip && std::find(originals.begin(), originals.end(), clip) == originals.end(), "native clip clone is shared");
            r::FieldObjectSet(copy, trackParentField, nullptr);
            r::Call(clearChildren, copy); r::Call(clearMarkers, copy);
            r::Set(setMuted, copy, r::Read<bool>(mutedHierarchy, source));
            for (void* clip : cloned)
            {
                void* originalAsset = roots.Keep(r::Call(clipAsset, clip));
                r::Require(r::Is(originalAsset, equipment ? equipmentClipClass : bpmClipClass), "non-light clip on native light track");
                void* clipCopy = KeepClone(originalAsset); void* args[]{clipCopy}; r::Call(setClipAsset, clip, args);
                r::FieldObjectSet(clip, clipParentField, copy);
                if (equipment)
                {
                    r::FieldObjectSet(clipCopy, clipControllerField, nullptr);
                    r::FieldObjectSet(clipCopy, clipTrackField, copy);
                    needsBpm = needsBpm || r::Read<bool>(clipUseBpm, clipCopy);
                }
                ++clips;
            }
            if (!r::Read<bool>(mutedHierarchy, source)) equipment ? ++equipmentTracks : ++bpmTracks;
            return copy;
        }
        // pairs: optional action-scoped (original, copy) list for binding copy.
        void Build(void* source, std::vector<std::pair<void*, void*>>* pairs = nullptr)
        {
            r::Require(!asset && r::Is(source, timelineClass), "source is not an unused native timeline");
            r::Arena roots;
            void* type = r::TypeObject(timelineClass, roots); void* args[]{type}; asset = r::Root(r::Call(createAsset, nullptr, args));
            const auto tracks = r::Items(roots.Keep(r::Call(flattenedTracks, source)), 4096);
            for (void* track : tracks) if (Allowed(track))
            {
                r::Require(!copies.contains(track), "duplicate native flattened track");
                copies.emplace(track, CopyTrack(track, roots));
            }
            r::Require(equipmentTracks > 0 && clips > 0 && (!needsBpm || bpmTracks > 0), "native equipment/BPM tracks missing or muted");
            for (void* track : tracks)
            {
                const auto found = copies.find(track); if (found == copies.end()) continue;
                void* parent = roots.Keep(r::Call(trackParent, track));
                unsigned depth = 0;
                while (parent && parent != source && !copies.contains(parent))
                {
                    r::Require(++depth <= 64 && r::Is(parent, groupClass), "unsupported native light parent track");
                    parent = roots.Keep(r::Call(trackParent, parent));
                }
                void* addArgs[]{found->second};
                if (parent && copies.contains(parent)) r::Call(addChild, copies.at(parent), addArgs);
                else r::Call(addTrack, asset.Get(), addArgs);
            }
            if (pairs) pairs->assign(copies.begin(), copies.end());
            copies.clear(); // No raw managed instances survive this action.
        }
        bool Clear()
        {
            copies.clear();
            // The graph/controller must already be stopped and disposed.
            for (auto& value : ownedAssets) Destroy(value);
            ownedAssets.clear(); Destroy(asset); equipmentTracks = bpmTracks = clips = 0; needsBpm = false;
            return true;
        }
    };
}
