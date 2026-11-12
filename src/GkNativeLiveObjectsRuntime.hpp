#pragma once
namespace NativeLiveRuntime
{
    inline constexpr size_t kMaxGlobalComponents = 100000, kMaxStageComponents = 16384;
    inline std::vector<void*> Components(void* klass, int scene, r::Arena& roots)
    {
        void* type = r::TypeObject(klass, roots); bool inactive = true;
        void* args[]{type, &inactive};
        const auto values = r::Items(roots.Keep(r::Call(StageLoader::findAll, nullptr, args)), kMaxGlobalComponents);
        std::vector<void*> result;
        for (void* value : values)
        {
            if (!value || !r::Alive(value)) continue;
            void* go = roots.Keep(r::Call(StageLoader::componentGo, value));
            if (r::Read<int>(StageLoader::goScene, go) == scene) result.push_back(value);
        }
        r::Require(result.size() <= kMaxStageComponents, "native stage component snapshot exceeds bound");
        return result;
    }
    struct ComponentId
    {
        int component{}, go{}, scene{};
        bool operator==(const ComponentId&) const = default;
    };
    inline ComponentId Identity(void* value, r::Arena& roots)
    {
        void* go = roots.Keep(r::Call(StageLoader::componentGo, value));
        return {r::Read<int>(StageLoader::instanceId, value), r::Read<int>(StageLoader::instanceId, go),
            r::Read<int>(StageLoader::goScene, go)};
    }
}
