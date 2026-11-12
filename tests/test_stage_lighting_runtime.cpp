// Exercise the production Unity adapter using synthetic typed IL2CPP arrays.
#include "GkStageLighting.hpp"
#include "GkStageLoadPolicy.hpp"
#include "GkExactMethod.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

using HMODULE = void*;
void* GetProcAddress(HMODULE, const char*) { return nullptr; }
void* FindClass(const char*, const char*, const char*) { return nullptr; }
namespace GakuMotion::host { template<class... Args> void Info(const char*, Args...) {} }

namespace Fixture
{
    namespace lighting = GakuMotion::stage_light;
    enum Method { FindAll, ComponentGo, InstanceId, Scene, Enabled, SetEnabled,
        Color, SetColor, Intensity, SetIntensity, Active, UpdateBeam, Count };
    int methods[Count]{}, fields[6]{0, 1, 2, 3, 4, 5}, lightType{}, beamType{}, goType{};
    const void* Token(Method value) { return &methods[value]; }
    struct Object { int id{}, type{}; };
    struct Go : Object { int scene{17}; bool active{true}; };
    struct Component : Object { Go* go{}; lighting::Values value; };
    struct Array { void* klass{}; void* monitor{}; void* bounds{}; uintptr_t length{}; void* items[8]{}; };
    static_assert(offsetof(Array, items) == 32);
    Array lights, beams;
    Go lightGo, beamGo, foreignGo;
    Component light, beam, foreign;
    int pins{}, frees{}, setters{}, failures{}, beamUpdates{};
    bool mainThread{true}, enumerationFails{}, readFails{};
    int boxedInt{}; bool boxedBool{}; float boxedFloat{}; lighting::Color boxedColor{};
    lighting::Values Baseline() { return {{.2f, .3f, .4f, 1}, 2, 3, false, true, true, 1}; }
    void* Invoke(const void* method, void* target, void** args, void** exception)
    {
        *exception = nullptr;
        if (method == Token(FindAll))
        {
            if (enumerationFails) return nullptr;
            return args[0] == &lightType ? &lights : &beams;
        }
        if (method == Token(InstanceId)) { boxedInt = static_cast<Object*>(target)->id; return &boxedInt; }
        if (method == Token(ComponentGo)) return static_cast<Component*>(target)->go;
        if (method == Token(Scene)) { boxedInt = static_cast<Go*>(target)->scene; return &boxedInt; }
        if (method == Token(Active)) { boxedBool = static_cast<Go*>(target)->active; return &boxedBool; }
        auto& value = static_cast<Component*>(target)->value;
        if (method == Token(Enabled))
        { if (readFails) { *exception = &readFails; return nullptr; } boxedBool = value.enabled; return &boxedBool; }
        if (method == Token(Color)) { boxedColor = value.color; return &boxedColor; }
        if (method == Token(Intensity)) { boxedFloat = value.intensity; return &boxedFloat; }
        if (method == Token(UpdateBeam)) { ++beamUpdates; return nullptr; }
        ++setters;
        if (method == Token(SetEnabled)) value.enabled = *static_cast<bool*>(args[0]);
        if (method == Token(SetColor)) value.color = *static_cast<lighting::Color*>(args[0]);
        if (method == Token(SetIntensity)) value.intensity = *static_cast<float*>(args[0]);
        if (failures) { --failures; *exception = &failures; }
        return nullptr;
    }
    void FieldGet(void* object, void* field, void* output)
    {
        const auto& value = static_cast<Component*>(object)->value;
        switch (*static_cast<int*>(field))
        {
        case 0: *static_cast<lighting::Color*>(output) = value.color; break;
        case 1: *static_cast<float*>(output) = value.intensity; break;
        case 2: *static_cast<float*>(output) = value.outside; break;
        case 3: *static_cast<bool*>(output) = value.colorFromLight; break;
        case 4: *static_cast<bool*>(output) = value.intensityFromLight; break;
        case 5: *static_cast<int*>(output) = value.colorMode; break;
        }
    }
    void FieldSet(void* object, void* field, void* input)
    {
        auto& value = static_cast<Component*>(object)->value;
        ++setters;
        switch (*static_cast<int*>(field))
        {
        case 0: value.color = *static_cast<lighting::Color*>(input); break;
        case 1: value.intensity = *static_cast<float*>(input); break;
        case 2: value.outside = *static_cast<float*>(input); break;
        case 3: value.colorFromLight = *static_cast<bool*>(input); break;
        case 4: value.intensityFromLight = *static_cast<bool*>(input); break;
        case 5: value.colorMode = *static_cast<int*>(input); break;
        }
    }
}

namespace RuntimeFixture
{
    inline struct { decltype(&Fixture::Invoke) invoke = Fixture::Invoke; } g_faceRuntime;
    inline struct { void* (*unbox)(void*) = [](void* value) { return value; }; } g_safeBones;
    inline void* (*il2cpp_class_get_type)(void*) = [](void* value) { return value; };
    inline void* (*il2cpp_type_get_object)(void*) = [](void* value) { return value; };
    namespace StageLoader
    {
        using Handle = void*;
        inline Handle (*newHandle)(void*, bool) = [](void* value, bool) { ++Fixture::pins; return value; };
        inline void (*freeHandle)(Handle) = [](void*) { ++Fixture::frees; };
        inline uintptr_t (*arrayLength)(void*) = [](void* value) { return static_cast<Fixture::Array*>(value)->length; };
        inline int (*valueSize)(void*, uint32_t*){};
        inline bool (*assignable)(void*, void*){};
        inline GakuMotion::ExactMethodMetadata metadata;
        inline void* goClass = &Fixture::goType;
        inline const void *findAll = Fixture::Token(Fixture::FindAll), *componentGo = Fixture::Token(Fixture::ComponentGo),
            *instanceId = Fixture::Token(Fixture::InstanceId), *goScene = Fixture::Token(Fixture::Scene);
        void* Invoke(const void* method, void* target = nullptr, void** args = nullptr)
        { void* exception = nullptr; void* result = Fixture::Invoke(method, target, args, &exception); return exception ? nullptr : result; }
    }
    inline std::atomic<bool> g_runtimeReady{true};
    inline bool g_activeSceneVerified{true};
    inline int g_verifiedActiveSceneHandle{17};
    inline GakuMotion::injection::OwnerSnapshot g_sceneOwner;
    inline std::string g_verifiedOwnerSceneName = "Photography";
    namespace SceneOwnerRuntime { inline std::string cachedActiveScene = "env_3d_live_all001-00-noon"; }
    bool ProtectionOnUnityThread() { return Fixture::mainThread; }
#include "GkStageLightingRuntime.hpp"
}

namespace
{
    using namespace Fixture;
    using namespace RuntimeFixture;
    namespace runtime = RuntimeFixture::StageLightingRuntime;
    int passed{}, failed{};
    void Check(bool ok, const char* description)
    { if (ok) ++passed; else { ++failed; std::printf("[FAIL] %s\n", description); } }
    void Reset()
    {
        pins = frees = setters = failures = beamUpdates = 0;
        mainThread = true; enumerationFails = readFails = false;
        lightGo = {{101, 0}, 17, true}; beamGo = {{102, 0}, 17, true}; foreignGo = {{103, 0}, 99, true};
        light = {{1, 1}, &lightGo, Baseline()}; light.value.outside = 0;
        light.value.colorFromLight = light.value.intensityFromLight = false; light.value.colorMode = 0;
        beam = {{2, 2}, &beamGo, Baseline()}; foreign = {{3, 1}, &foreignGo, light.value};
        lights = {}; lights.length = 2; lights.items[0] = &light; lights.items[1] = &foreign;
        beams = {}; beams.length = 1; beams.items[0] = &beam;
        g_runtimeReady = g_activeSceneVerified = true;
        g_sceneOwner = {true, 0x1234, 2, true, GakuMotion::injection::OwnerRole::Photography, false};
        GakuMotion::stage_load::receipt.Clear(); GakuMotion::stage_load::receipt.Record(g_sceneOwner, SceneOwnerRuntime::cachedActiveScene);
        runtime::owner = {};
        runtime::lightClass = &lightType; runtime::beamClass = &beamType;
        runtime::lightReady = runtime::beamReady = true;
        runtime::getEnabled = Token(Enabled); runtime::setEnabled = Token(SetEnabled);
        runtime::getColor = Token(Color); runtime::setColor = Token(SetColor);
        runtime::getIntensity = Token(Intensity); runtime::setIntensity = Token(SetIntensity);
        runtime::activeInHierarchy = Token(Active); runtime::updateBeam = Token(UpdateBeam);
        runtime::fieldGet = FieldGet; runtime::fieldSet = FieldSet;
        for (int i = 0; i < 6; ++i) runtime::beamFields[i] = &fields[i];
        runtime::controller = {}; runtime::controller.api = {runtime::Begin, runtime::End, runtime::ReadValues, runtime::WriteValues};
    }
}
int main()
{
    std::string notice;
    Reset(); const auto originalLight = light.value, originalBeam = beam.value;
    Check(runtime::Update({lighting::Mode::Cycle}, 2, notice) && runtime::controller.lights == 1 && runtime::controller.beams == 1,
        "production adapter drives a real light and VLB beam through typed rooted arrays");
    Check(light.value != originalLight && beam.value != originalBeam && foreign.value == originalLight && beamUpdates > 0,
        "actual adapter setters update beam geometry while other loaded scenes remain untouched");
    Check(pins == frees && runtime::pins.empty(), "all managed enumeration roots are action-scoped");
    Check(runtime::Restore() && light.value == originalLight && beam.value == originalBeam, "production readback verifies exact baseline restoration");
    for (int denial = 0; denial < 5; ++denial)
    {
        Reset();
        if (denial == 0) GakuMotion::stage_load::receipt.Clear();
        if (denial == 1) g_activeSceneVerified = false;
        if (denial == 2) g_runtimeReady = false;
        if (denial == 3) ++g_sceneOwner.presenterIdentity;
        if (denial == 4) mainThread = false;
        runtime::Update({lighting::Mode::Cycle}, 2, notice);
        Check(setters == 0 && pins == frees, "unreceipted/unverified/unready/replaced/worker contexts perform zero light writes");
    }
    Reset(); runtime::Update({lighting::Mode::Wash}, 1, notice);
    mainThread = false;
    Check(!runtime::Restore() && runtime::controller.HasChanges(), "worker shutdown retains pending restoration without Unity calls");
    mainThread = true; runtime::PollRestoration(false);
    Check(!runtime::controller.HasChanges() && light.value == originalLight && beam.value == originalBeam,
        "host-only tick restores the stage even without another actor callback");
    Reset(); failures = 1;
    Check(!runtime::Update({lighting::Mode::Cycle}, 1, notice) && !runtime::controller.HasChanges() && light.value == originalLight,
        "managed setter exception restores a partially changed light using the production adapter");
    Reset(); enumerationFails = true;
    Check(!runtime::Update({lighting::Mode::Wash}, 1, notice) && setters == 0 && pins == frees, "null typed enumeration prevents light writes");
    Reset(); readFails = true;
    Check(!runtime::Update({lighting::Mode::Wash}, 1, notice) && setters == 0 && pins == frees, "failed enabled-state baseline prevents all writes");
    std::printf("Stage lighting runtime adapter: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
