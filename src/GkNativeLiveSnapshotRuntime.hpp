#pragma once
#include "GkNativeLiveObjectsRuntime.hpp"
#include "GkNativeLiveMaterialsRuntime.hpp"
#include "GkNativeLiveParticlesRuntime.hpp"
namespace NativeLiveRuntime
{
    struct NativeSnapshot
    {
        using Vec3 = std::array<float, 3>;
        using Quat = std::array<float, 4>;
        struct Transform { ComponentId id; Vec3 position, scale; Quat rotation; bool active; };
        struct Serialized { ComponentId id; void* klass; std::string json; };
        struct Light { ComponentId id; std::array<float, 6> values; bool temperature; };
        std::vector<Transform> transforms;
        std::vector<Serialized> serialized;
        std::vector<Light> lights;
        MaterialSnapshot materialBlocks;
        ParticleSnapshot particles;
        int scene{};
        std::array<const void*, 6> getLight{}, setLight{};
        const void *getTemperature{}, *setTemperature{};

        void ResolveProperties()
        {
            const char* names[]{"range", "spotAngle", "innerSpotAngle", "bounceIntensity", "colorTemperature", "shadowStrength"};
            for (size_t i = 0; i < getLight.size(); ++i)
            {
                getLight[i] = r::Find(StageLightingRuntime::lightClass, (std::string("get_") + names[i]).c_str(), false, {0x0c}, {});
                setLight[i] = r::Find(StageLightingRuntime::lightClass, (std::string("set_") + names[i]).c_str(), false, {0x01}, {{0x0c}});
                r::Require(getLight[i] && setLight[i], "native Light property snapshot unavailable");
            }
            getTemperature = r::Find(StageLightingRuntime::lightClass, "get_useColorTemperature", false, {0x02}, {});
            setTemperature = r::Find(StageLightingRuntime::lightClass, "set_useColorTemperature", false, {0x01}, {{0x02}});
            r::Require(getTemperature && setTemperature, "native Light temperature snapshot unavailable");
        }
        void Capture(int handle)
        {
            r::Require(!scene, "previous native snapshot has not been restored");
            ResolveProperties(); scene = handle;
            materialBlocks.Capture(scene);
            particles.Capture(scene);
            r::Arena roots;
            size_t serializedBytes{};
            constexpr size_t maxSerializedBytes = 32 * 1024 * 1024;
            for (void* value : Components(StageLoader::transformClass, scene, roots))
            {
                void* go = roots.Keep(r::Call(StageLoader::componentGo, value));
                transforms.push_back({Identity(value, roots), r::Read<Vec3>(getLocalPosition, value),
                    r::Read<Vec3>(getLocalScale, value), r::Read<Quat>(getLocalRotation, value),
                    r::Read<bool>(StageLoader::activeSelf, go)});
            }
            for (void* klass : {environmentClass, StageLightingRuntime::beamClass})
            {
                r::Require(klass != nullptr, "native environment snapshot type unavailable");
                for (void* value : Components(klass, scene, roots))
                {
                    bool pretty = false; void* args[]{value, &pretty};
                    auto json = r::Text(roots.Keep(r::Call(toJson, nullptr, args)), 262144);
                    r::Require(json.size() <= maxSerializedBytes - serializedBytes, "native script snapshot exceeds total bound");
                    serializedBytes += json.size();
                    serialized.push_back({Identity(value, roots), klass, std::move(json)});
                }
            }
            for (void* value : Components(StageLightingRuntime::lightClass, scene, roots))
            {
                Light light{Identity(value, roots)};
                for (size_t i = 0; i < getLight.size(); ++i) light.values[i] = r::Read<float>(getLight[i], value);
                light.temperature = r::Read<bool>(getTemperature, value); lights.push_back(light);
            }
            r::Require(StageLightingRuntime::controller.Capture(scene), "native Light/VLB baseline capture failed");
        }
        template<class T, class Writer> bool RestoreRows(std::vector<T>& rows, void* klass, Writer write)
        {
            if (rows.empty()) return true;
            r::Arena roots; const auto fresh = Components(klass, scene, roots);
            std::map<int, std::pair<ComponentId, void*>> indexed;
            for (void* value : fresh)
            { const auto id = Identity(value, roots); indexed.emplace(id.component, std::pair{id, value}); }
            bool ok = true;
            for (auto it = rows.begin(); it != rows.end();)
            {
                const auto entry = indexed.find(it->id.component);
                void* found = entry != indexed.end() && entry->second.first == it->id ? entry->second.second : nullptr;
                try
                {
                    if (found) write(found, *it, roots);
                    it = rows.erase(it); // Confirmed absent identity is gone.
                }
                catch (...) { ok = false; ++it; }
            }
            return ok;
        }
        bool Restore()
        {
            if (!scene) return StageLightingRuntime::Restore();
            bool ok = true;
            // Restore serialized scripts before built-in components. No shared
            // timeline/clip/material asset is serialized or overwritten.
            std::map<int, void*> freshSerialized;
            r::Arena roots;
            for (void* klass : {environmentClass, StageLightingRuntime::beamClass})
                for (void* value : Components(klass, scene, roots))
                    freshSerialized.emplace(r::Read<int>(StageLoader::instanceId, value), value);
            for (auto it = serialized.begin(); it != serialized.end();)
            {
                try
                {
                    const auto found = freshSerialized.find(it->id.component);
                    if (found != freshSerialized.end() && Identity(found->second, roots) == it->id)
                    {
                        void* json = r::String(it->json, roots);
                        void* type = r::TypeObject(g_faceRuntime.objectClass(found->second), roots);
                        void* args[]{json, found->second, type}; r::Call(fromJson, nullptr, args);
                        if (it->klass == StageLightingRuntime::beamClass) r::Call(StageLightingRuntime::updateBeam, found->second);
                    }
                    it = serialized.erase(it);
                }
                catch (...) { ok = false; ++it; }
            }
            ok = RestoreRows(lights, StageLightingRuntime::lightClass, [&](void* value, const Light& row, r::Arena&) {
                for (size_t i = 0; i < setLight.size(); ++i) r::Set(setLight[i], value, row.values[i]);
                r::Set(setTemperature, value, row.temperature);
            }) && ok;
            ok = StageLightingRuntime::Restore() && ok;
            ok = materialBlocks.Restore() && ok;
            ok = RestoreRows(transforms, StageLoader::transformClass, [&](void* value, const Transform& row, r::Arena& roots) {
                r::Set(setLocalPosition, value, row.position); r::Set(setLocalRotation, value, row.rotation); r::Set(setLocalScale, value, row.scale);
                void* go = roots.Keep(r::Call(StageLoader::componentGo, value)); r::Set(StageLoader::setActive, go, row.active);
            }) && ok;
            ok = particles.Restore() && ok;
            if (ok && serialized.empty() && lights.empty() && transforms.empty()) scene = 0;
            return !scene;
        }
    };
}
