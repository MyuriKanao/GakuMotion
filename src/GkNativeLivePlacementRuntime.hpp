#pragma once
// Keep the native Photography actor/camera rig intact. A replacement Live and
// its owned lighting scene share a translated origin instead of inheriting the
// original Photography background's (potentially underground) performer origin.
namespace NativeLiveRuntime
{
    struct StagePlacement
    {
        using Vec3 = std::array<float, 3>;
        struct Parent { r::Root go, transform; int scene{}; };
        struct Child { r::Root transform; ComponentId identity; size_t parent{}; };
        std::array<Parent, 2> parents;
        std::vector<Child> children;
        const void *worldPosition{}, *setParent{}, *childCount{};
        Vec3 offset{};

        void Resolve()
        {
            if (worldPosition && setParent && childCount) return;
            void* vector = r::Unity("Vector3");
            worldPosition = r::Find(StageLoader::transformClass, "get_position", false, {0x11, vector}, {});
            setParent = r::Find(StageLoader::transformClass, "SetParent", false, {0x01},
                {{0x12, StageLoader::transformClass}, {0x02}});
            childCount = r::Find(StageLoader::transformClass, "get_childCount", false, {0x08}, {});
            r::Require(vector && worldPosition && setParent && childCount, "native stage origin interface unavailable");
        }
        static bool Finite(const Vec3& value)
        { return std::all_of(value.begin(), value.end(), [](float v) { return std::isfinite(v); }); }
        Vec3 Position(void* component, r::Arena& roots)
        {
            void* go = roots.Keep(r::Call(StageLoader::componentGo, component));
            void* transform = roots.Keep(r::Call(StageLoader::goTransform, go));
            const auto value = r::Read<Vec3>(worldPosition, transform);
            r::Require(Finite(value), "native stage origin is non-finite");
            return value;
        }
        Vec3 SourceOrigin(int scene, r::Arena& roots)
        {
            const auto actors = Components(auxiliary::bindings.virtualActor, scene, roots);
            if (actors.empty()) return {}; // No performer marker: use the scene origin, not guessed mesh bounds.
            if (actors.size() == 1) return Position(actors.front(), roots);
            void* primary{};
            for (void* actor : actors)
            {
                if (r::Text(roots.Keep(r::Call(StageLoader::objectName, actor)), 1024) != "VirtualActor_00") continue;
                r::Require(!primary, "native stage primary performer is ambiguous");
                primary = actor;
            }
            r::Require(primary != nullptr, "native stage primary performer is unavailable");
            return Position(primary, roots);
        }
        void Attach(int scene, size_t index, r::Arena& roots)
        {
            // Enumerate before creating the wrapper, and move only top-level
            // objects in this exact scene. Local timeline animation stays local.
            const auto transforms = Components(StageLoader::transformClass, scene, roots);
            auto& parent = parents[index]; parent.scene = scene;
            NewGo(parent.go, index ? "GakuMotion.NativeLive.SourceOrigin.Plugin" : GakuMotion::native_live::kStageOriginRoot, scene, roots);
            parent.transform = r::Root(r::Call(StageLoader::goTransform, parent.go.Get()));
            r::Set(setLocalPosition, parent.transform.Get(), offset);
            for (void* transform : transforms)
            {
                if (roots.Keep(r::Call(StageLoader::parent, transform))) continue;
                children.push_back({r::Root(transform), Identity(transform, roots), index});
                bool keepWorld = false;
                void* args[]{parent.transform.Get(), &keepWorld}; r::Call(setParent, transform, args);
                r::Require(r::Call(StageLoader::parent, transform) == parent.transform.Get(), "native stage origin parenting incomplete");
            }
            r::Set(StageLoader::setActive, parent.go.Get(), true);
        }
        void Apply(int stageScene, int sourceScene, const Vec3& photographyOrigin)
        {
            r::Require(children.empty() && !parents[0].go && !parents[1].go && stageScene && sourceScene && stageScene != sourceScene,
                "native stage origin ownership unavailable");
            Resolve(); r::Arena roots;
            r::Require(Finite(photographyOrigin), "photography stage origin is non-finite");
            const auto source = SourceOrigin(sourceScene, roots);
            for (size_t i = 0; i < offset.size(); ++i) offset[i] = photographyOrigin[i] - source[i];
            r::Require(Finite(offset), "photography stage offset is non-finite");
            constexpr float kOriginTolerance = 1e-4f;
            if (std::all_of(offset.begin(), offset.end(), [](float v) { return std::fabs(v) <= kOriginTolerance; })) return;
            Attach(stageScene, 0, roots); Attach(sourceScene, 1, roots);
        }
        bool Restore()
        {
            bool ok = true;
            r::Arena roots;
            for (auto it = children.begin(); it != children.end();)
            {
                try
                {
                    void* transform = it->transform.Get();
                    if (r::Alive(transform))
                    {
                        r::Require(Identity(transform, roots) == it->identity, "native stage origin child identity changed");
                        void* current = roots.Keep(r::Call(StageLoader::parent, transform));
                        r::Require(!current || (parents[it->parent].transform && current == parents[it->parent].transform.Get()),
                            "native stage origin child was reparented externally");
                        // Detaching from a hidden wrapper must never reactivate a
                        // source camera, actor, media player or lighting receiver.
                        if (it->parent)
                            r::Set(StageLoader::setActive, roots.Keep(r::Call(StageLoader::componentGo, transform)), false);
                        if (current)
                        {
                            bool keepWorld = false; void* args[]{nullptr, &keepWorld}; r::Call(setParent, transform, args);
                            r::Require(!r::Call(StageLoader::parent, transform), "native stage origin restoration incomplete");
                        }
                    }
                    it = children.erase(it);
                }
                catch (...) { ok = false; ++it; }
            }
            if (!children.empty()) return false;
            for (auto& parent : parents)
            {
                try
                {
                    if (parent.go && r::Alive(parent.go.Get()))
                        r::Require(r::Read<int>(childCount, parent.transform.Get()) == 0, "native stage origin has untracked children");
                    Destroy(parent.go); parent.transform.Reset(); parent.scene = 0;
                }
                catch (...) { ok = false; }
            }
            return ok;
        }
    };
}
