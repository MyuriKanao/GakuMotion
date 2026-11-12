#pragma once
namespace NativeLiveRuntime
{
    // Only the disposable scene proven owned by SourceScene is supplied here.
    // Objects stay in that scene; nothing is adopted from or moved into the
    // game's stage. Release hides every root before the owned scene unloads.
    struct SourceComposition
    {
        struct Global { int id{}; r::Root texture; };
        int scene{};
        unsigned mask{};
        bool sourceReceiver{}, captured{}, activated{};
        std::vector<plan::Node> nodes;
        std::map<int, size_t> indices;
        std::set<int> parents;
        std::map<int, r::Root> roots;
        std::map<int, bool> original;
        std::map<int, unsigned> active; // Planned active root -> admitted features.
        std::vector<r::Root> lifted;
        std::vector<Global> globals;
        r::Root receiver;
        ComponentId receiverIdentity;
        plan::Result result;
        int rigRoots{}, environmentRoots{}, monitorRoots{}, audienceRoots{};

        int Root(int id) const
        {
            for (size_t depth = 0; id && depth <= plan::kMaxNodes; ++depth)
            {
                const auto found = indices.find(id);
                if (found == indices.end()) return 0; // Created after capture: never admitted.
                const int parent = nodes[found->second].parent;
                if (!parent) return id;
                id = parent;
            }
            return 0;
        }
        // Admitted features of the active planned root owning a source object.
        unsigned Features(int goId) const
        {
            const auto found = active.find(Root(goId));
            return found == active.end() ? 0u : found->second;
        }
        // A bare child director (e.g. MasterTimeline/Light) may become a root
        // of its own; its parent controller, cameras and audio stay hidden.
        bool Liftable(int goId) const
        {
            const auto found = indices.find(goId);
            if (found == indices.end() || parents.contains(goId)) return false;
            const auto& node = nodes[found->second];
            return node.parent && !node.features && !node.forbidden && !node.camera && !node.effect && !node.receiver;
        }
        plan::Node& NodeOf(void* component, r::Arena& pins)
        {
            void* go = r::Is(component, goClass) ? component : pins.Keep(r::Call(StageLoader::componentGo, component));
            const auto found = indices.find(r::Read<int>(StageLoader::instanceId, go));
            r::Require(found != indices.end(), "composition component lacks source transform identity");
            return nodes[found->second];
        }
        template<class F> void Each(void* klass, F mark)
        {
            r::Arena pins;
            for (void* value : Components(klass, scene, pins)) mark(value, NodeOf(value, pins));
        }
        void Mark(void* klass, unsigned feature) { Each(klass, [&](void*, plan::Node& node) { node.features |= feature; }); }
        void Forbid(void* klass) { Each(klass, [&](void*, plan::Node& node) { node.forbidden = true; }); }
        void Silence(void* klass, bool disable, bool director = false, bool audio = false)
        {
            r::Arena pins;
            for (void* value : Components(klass, scene, pins))
            {
                // Source-only objects in the disposable scene; never re-enabled.
                if (disable) r::Set(StageLightingRuntime::setEnabled, value, false);
                if (director)
                {
                    // Hosted directors keep their component enabled for Evaluate.
                    r::Set(setAutoPlay, value, false); r::Set(setManual, value, manualMode);
                    r::Call(directorStop, value);
                }
                if (audio) r::Call(auxiliary::bindings.audioStop, value);
            }
        }
        void ReadHierarchy()
        {
            r::Arena pins;
            const auto transforms = Components(StageLoader::transformClass, scene, pins);
            r::Require(!transforms.empty() && transforms.size() <= plan::kMaxNodes, "composition source hierarchy unavailable or too large");
            for (void* transform : transforms)
            {
                void* go = pins.Keep(r::Call(StageLoader::componentGo, transform));
                const int id = r::Read<int>(StageLoader::instanceId, go);
                void* parent = pins.Keep(r::Call(StageLoader::parent, transform));
                int parentId{};
                if (parent)
                {
                    void* parentGo = pins.Keep(r::Call(StageLoader::componentGo, parent));
                    r::Require(r::Read<int>(StageLoader::goScene, parentGo) == scene, "composition parent leaves owned source scene");
                    parentId = r::Read<int>(StageLoader::instanceId, parentGo);
                    parents.insert(parentId);
                }
                r::Require(id && indices.emplace(id, nodes.size()).second, "composition source identity is duplicated");
                plan::Node node{id, parentId};
                if (!parentId)
                {
                    roots.emplace(id, r::Root(go));
                    node.features = plan::NameHint(r::Text(pins.Keep(r::Call(StageLoader::objectName, go)), 1024));
                }
                nodes.push_back(node);
            }
        }
        void Capture(int sourceScene, const std::map<int, bool>& states, unsigned layers, bool useSourceReceiver)
        {
            r::Require(sourceScene && !scene && !captured && !activated, "previous native composition still held");
            auto& api = auxiliary::bindings; api.Resolve();
            scene = sourceScene; original = states; sourceReceiver = useSourceReceiver;
            // With a paired stage receiver the stage owns the light rig; source
            // rig roots stay hidden as before and only optional layers apply.
            mask = (useSourceReceiver ? plan::Rig : 0u) | (layers & (plan::Environment | plan::Monitor | plan::Audience));
            ReadHierarchy();
            Each(receiverClass, [&](void*, plan::Node& node) { node.features |= plan::Rig; node.receiver = true; });
            // EquipmentValue objects are the equipment tracks' binding targets and
            // carry their own Animators, so their root must be active. Its source
            // controller is disabled and its source director stopped below.
            for (void* klass : {StageLightingRuntime::lightClass, StageLightingRuntime::beamClass, environmentClass,
                api.lightAnchor, api.equipmentValue})
                Mark(klass, plan::Rig);
            Each(api.volume, [&](void* value, plan::Node& node) {
                if (!r::Is(value, api.crowd) && !r::Is(value, api.audienceVolume)) node.features |= plan::Environment; });
            Mark(api.renderer, plan::Environment); Mark(api.decal, plan::Environment);
            Mark(api.monitor, plan::Monitor); Mark(api.videoPlayer, plan::Monitor);
            for (void* klass : {api.crowd, api.audienceVolume, api.mob}) Mark(klass, plan::Audience);
            for (void* klass : {api.actor, api.virtualActor, api.virtualCamera, api.audio, api.listener, api.liveController, api.criPlayer})
                Forbid(klass);
            Each(api.particle, [&](void*, plan::Node& node) { node.effect = true; });
            Each(api.camera, [&](void* value, plan::Node& node) {
                r::Arena pins;
                (pins.Keep(r::Call(api.cameraTarget, value)) ? node.camera : node.forbidden) = true; });
            Each(api.canvas, [&](void* value, plan::Node& node) {
                const int mode = r::Read<int>(api.canvasMode, value);
                (mode == api.worldCanvas || mode == api.cameraCanvas ? node.camera : node.forbidden) = true; });
            // Isolate already deactivated every root. Keep source-only runtime
            // parts silent even if an admitted root is enabled later.
            Silence(equipmentClass, true); Silence(directorClass, false, true); Silence(api.audio, true, false, true);
            for (void* klass : {api.listener, api.actor, api.virtualActor, api.virtualCamera, api.liveController})
                Silence(klass, true);
            result = plan::Plan(nodes, mask, original, sourceReceiver);
            for (const auto& root : result.roots) if (root.active) active.emplace(root.id, root.features);
            if (sourceReceiver)
            {
                r::Arena pins;
                const auto receivers = Components(receiverClass, scene, pins);
                r::Require(receivers.size() == 1 && result.receiver, "owned source equipment receiver is absent, ambiguous or hidden");
                receiver = r::Root(receivers.front()); receiverIdentity = Identity(receiver.Get(), pins);
            }
            captured = true;
        }
        void* Receiver() const
        {
            r::Require(receiver && r::Alive(receiver.Get()), "owned source receiver expired");
            r::Arena pins;
            r::Require(Identity(receiver.Get(), pins) == receiverIdentity, "owned source receiver identity changed");
            return receiver.Get();
        }
        void CaptureGlobals()
        {
            // The screen's render texture may be published as a shader global.
            // Record the prior values of the first slots and restore them.
            auto& api = auxiliary::bindings;
            for (int index = 0; index < 4; ++index)
            {
                r::Arena pins; void* args[]{&index};
                std::string name;
                try { name = r::Text(pins.Keep(r::Call(api.textureName, nullptr, args)), 256); }
                catch (...) { break; } // No further named slot.
                if (name.empty()) continue;
                void* text = r::String(name, pins); void* idArgs[]{text};
                int id = r::Read<int>(api.propertyId, nullptr, idArgs);
                void* getArgs[]{&id};
                void* texture = r::Call(api.getGlobalTexture, nullptr, getArgs);
                Global global{id};
                if (texture) global.texture = r::Root(texture);
                globals.push_back(std::move(global));
            }
        }
        bool RestoreGlobals()
        {
            auto& api = auxiliary::bindings;
            bool ok = true;
            for (auto it = globals.end(); it != globals.begin();)
            {
                --it;
                try
                {
                    void* texture = it->texture ? it->texture.Get() : nullptr;
                    if (texture && !r::Alive(texture)) texture = nullptr;
                    void* args[]{&it->id, texture}; r::Call(api.setGlobalTexture, nullptr, args);
                    it = globals.erase(it);
                }
                catch (...) { ok = false; }
            }
            return ok;
        }
        void Activate(const std::vector<r::Root>& liftDirectors)
        {
            r::Require(captured && !activated, "composition activation is closed");
            // An older paired stage may own the whole rig; then nothing in the
            // source needs to be shown and only equipment tracks are driven.
            activated = true; // Hide() is the cleanup for a partial activation.
            CaptureGlobals();
            auto& api = auxiliary::bindings;
            // The external player owns sound; the screen video stays silent.
            Each(api.videoPlayer, [&](void* value, plan::Node& node) {
                if (Features(node.id) & plan::Monitor) r::Set(api.setVideoAudio, value, api.noVideoAudio); });
            r::Arena pins;
            for (const auto& director : liftDirectors)
            {
                void* go = pins.Keep(r::Call(StageLoader::componentGo, director.Get()));
                r::Require(r::Read<int>(StageLoader::goScene, go) == scene && Liftable(r::Read<int>(StageLoader::instanceId, go)),
                    "lifted director is not a bare owned source child");
                void* transform = pins.Keep(r::Call(StageLoader::goTransform, go));
                lifted.emplace_back(go);
                void* args[]{nullptr}; r::Call(auxiliary::bindings.setParent, transform, args);
            }
            // SourceScene::Isolate already hid every root; enforce it here too so
            // an unplanned root can never stay visible.
            for (const auto& [id, root] : roots)
                if (!active.contains(id) && r::Alive(root.Get())) r::Set(StageLoader::setActive, root.Get(), false);
            for (const auto& [id, features] : active)
            {
                const auto found = roots.find(id);
                r::Require(found != roots.end(), "composition admitted root is absent");
                void* go = found->second.Get();
                r::Require(r::Alive(go) && r::Read<int>(StageLoader::instanceId, go) == id &&
                    r::Read<int>(StageLoader::goScene, go) == scene, "composition root identity changed");
                r::Set(StageLoader::setActive, go, true);
                if (features & plan::Rig) ++rigRoots;
                if (features & plan::Environment) ++environmentRoots;
                if (features & plan::Monitor) ++monitorRoots;
                if (features & plan::Audience) ++audienceRoots;
            }
            for (const auto& go : lifted) r::Set(StageLoader::setActive, go.Get(), true);
        }
        bool Hide()
        {
            if (!scene) return true;
            if (activated)
            {
                try
                {
                    // Fresh enumeration also covers lifted directors and helpers
                    // created at runtime by monitor or crowd components.
                    r::Arena pins;
                    for (void* transform : Components(StageLoader::transformClass, scene, pins))
                        if (!pins.Keep(r::Call(StageLoader::parent, transform)))
                            r::Set(StageLoader::setActive, pins.Keep(r::Call(StageLoader::componentGo, transform)), false);
                    activated = false;
                }
                catch (...) { return false; }
            }
            return RestoreGlobals();
        }
        bool Release()
        {
            if (!Hide()) return false;
            roots.clear(); lifted.clear(); receiver.Reset(); nodes.clear(); indices.clear(); parents.clear();
            original.clear(); active.clear(); result = {};
            scene = rigRoots = environmentRoots = monitorRoots = audienceRoots = 0;
            mask = 0; captured = sourceReceiver = false;
            return true;
        }
    };
}
