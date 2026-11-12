#pragma once
#include "GkNativeLive.hpp"
#include <map>
#include <memory>
#include <array>
#include <cstring>
#include <climits>
#include <cstdio>

namespace NativeFixture
{
    struct Class { int type; };
    enum Kind { Asset, EquipmentTrack, BpmTrack, OtherTrack, EquipmentClip, BpmClip, OtherClip, Group, Go, Director, Equipment, Receiver, Clip, Array, Box };
    inline Class classes[Box + 1]{};
    struct Object
    {
        Kind kind{}; int id{}, scene{}; bool alive{true}, active{true}, muted{}, bpm{}, graph{};
        Object *parent{}, *asset{}, *go{}, *receiver{}, *director{}, *clipController{}, *clipTrack{};
        std::vector<void*> items, children;
        std::array<char, 32> bytes{};
        double time{}, duration{60}; int manual{};
    };
    enum Op { Create, Clone, Tracks, Clips, ClearChildren, ClearMarkers, Muted, SetMuted, UseBpm, ClipAsset, SetClipAsset,
        Parent, AddChild, AddTrack, GetAsset, Duration, SetFixed, SetDurationMode, SetAuto, SetManual, SetWrap, SetAsset,
        EquipmentReceiver, Init, Bind, Play, Valid, Stop, Dispose, ClearReceiver, Time, Evaluate, Update, Late, ReceiverUpdate,
        InstanceId, ComponentGo, Scene, SetActive, Last };
    inline std::vector<std::unique_ptr<Object>> objects;
    inline std::vector<std::string> calls;
    inline bool shallowClone{}, mainThread{true}, restoreReady{true};
    inline int frame{1}, mutations{}, captures{}, restores{}, destroyed{};
    inline int parentField{}, trackParentField{}, directorField{}, receiverField{}, controllerField{}, clipTrackField{};
    inline void UpdateFn(void*, const void*) { calls.push_back("update"); }
    inline void LateFn(void*, const void*) { calls.push_back("late"); }
    inline void ReceiverFn(void*, const void*) { calls.push_back("receiver"); }
    struct Method { void* address{}; Op op; };
    inline std::array<Method, Last> methods;
    inline void InitializeMethods()
    {
        for (int i = 0; i <= Box; ++i) classes[i].type = i;
        for (int i = 0; i < Last; ++i) methods[i].op = static_cast<Op>(i);
        methods[Update].address = reinterpret_cast<void*>(UpdateFn);
        methods[Late].address = reinterpret_cast<void*>(LateFn);
        methods[ReceiverUpdate].address = reinterpret_cast<void*>(ReceiverFn);
    }
    inline const void* M(Op op) { return &methods[op]; }
    inline Object* O(void* value) { return static_cast<Object*>(value); }
    inline Object* New(Kind kind)
    {
        auto value = std::make_unique<Object>(); value->kind = kind; value->id = static_cast<int>(objects.size()) + 1;
        auto result = value.get(); objects.push_back(std::move(value)); return result;
    }
    template<class T> void* Boxed(T value)
    { auto box = New(Box); std::memcpy(box->bytes.data(), &value, sizeof(T)); return box; }
    inline void* ArrayOf(const std::vector<void*>& items) { auto array = New(Array); array->items = items; return array; }
    inline Object* Track(Kind kind, Object* parent, bool bpm = false)
    {
        auto track = New(kind); track->parent = parent;
        auto clip = New(Clip); auto asset = New(kind == EquipmentTrack ? EquipmentClip : BpmClip); asset->bpm = bpm;
        clip->parent = track; clip->asset = asset; track->items.push_back(clip); return track;
    }
    inline bool Is(void* value, void* klass)
    {
        if (!value || !klass) return false;
        const auto kind = O(value)->kind; const auto expected = static_cast<Class*>(klass)->type;
        return kind == expected || (expected == Asset && kind <= Group);
    }
    inline void* Call(const void* method, void* target = nullptr, void** args = nullptr)
    {
        if (!method) throw std::runtime_error("missing fixture method");
        auto value = O(target); const auto op = static_cast<const Method*>(method)->op;
        switch (op)
        {
        case Create: return New(Asset);
        case Clone:
        {
            auto source = O(args[0]); auto clone = New(source->kind); const auto id = clone->id; *clone = *source; clone->id = id;
            if (!shallowClone && source->kind <= OtherTrack)
                for (auto& item : clone->items) { auto clip = New(Clip); const auto clipId = clip->id; *clip = *O(item); clip->id = clipId; item = clip; }
            return clone;
        }
        case Tracks: return ArrayOf(value->items);
        case Clips: return ArrayOf(value->items);
        case ClearChildren: value->children.clear(); break;
        case ClearMarkers: break;
        case Muted: return Boxed(value->muted);
        case SetMuted: value->muted = *static_cast<bool*>(args[0]); ++mutations; break;
        case UseBpm: return Boxed(value->bpm);
        case ClipAsset: return value->asset;
        case SetClipAsset: value->asset = O(args[0]); ++mutations; break;
        case Parent: return value->parent;
        case AddTrack: value->items.push_back(args[0]); O(args[0])->parent = value; break;
        case AddChild: value->children.push_back(args[0]); O(args[0])->parent = value; break;
        case GetAsset: return value->asset;
        case Duration: return Boxed(value->duration);
        case SetFixed: value->duration = *static_cast<double*>(args[0]); break;
        case SetDurationMode: break;
        case SetAuto: break;
        case SetManual: value->manual = *static_cast<int*>(args[0]); break;
        case SetWrap: break;
        case SetAsset: value->asset = O(args[0]); break;
        case EquipmentReceiver: return value->receiver;
        case Init: calls.push_back("init"); break;
        case Bind: value->receiver = O(args[0]); calls.push_back("bind"); break;
        case Play: value->graph = true; calls.push_back("play"); break;
        case Valid: return Boxed(value->director && value->director->graph);
        case Stop: value->graph = false; calls.push_back("stop"); break;
        case Dispose: calls.push_back("dispose"); break;
        case ClearReceiver: calls.push_back("clear"); break;
        case Time: value->time = *static_cast<double*>(args[0]); calls.push_back("time"); break;
        case Evaluate: calls.push_back("evaluate"); break;
        case InstanceId: return Boxed(value->id);
        case ComponentGo: return value->go;
        case Scene: return Boxed(value->scene);
        case SetActive: value->active = *static_cast<bool*>(args[0]); calls.push_back(value->active ? "active" : "inactive"); break;
        default: throw std::runtime_error("unexpected fixture call");
        }
        return nullptr;
    }
}
using GakuStatus = int;
inline constexpr int GAKU_OK = 0;
struct GakuHookRequest { void* target; void* replacement; void** original; };
namespace GakuMotion::host
{
    inline struct
    {
        void* context{};
        GakuStatus (*acquireBatch)(void*, unsigned, const GakuHookRequest*) = [](void*, unsigned count, const GakuHookRequest* requests)
        { for (unsigned i = 0; i < count; ++i) *requests[i].original = requests[i].target; return GAKU_OK; };
        GakuStatus (*release)(void*, void*) = [](void*, void*) { return GAKU_OK; };
    } hooks;
}
inline bool ProtectionOnUnityThread() { return NativeFixture::mainThread; }
inline int (*g_faceUnityFrame)() = [] { return NativeFixture::frame; };
namespace StageLoader
{
    inline const void *instanceId = NativeFixture::M(NativeFixture::InstanceId), *componentGo = NativeFixture::M(NativeFixture::ComponentGo),
        *goScene = NativeFixture::M(NativeFixture::Scene), *setActive = NativeFixture::M(NativeFixture::SetActive);
}
namespace NativeLiveRuntime
{
    namespace r
    {
        using namespace NativeFixture;
        inline void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
        struct Root
        {
            void* value{}; Root() = default; explicit Root(void* input) : value(input) { Require(input != nullptr, "root missing"); }
            Root(const Root&) = delete; Root& operator=(const Root&) = delete;
            Root(Root&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
            Root& operator=(Root&& other) noexcept { value = std::exchange(other.value, nullptr); return *this; }
            void* Get() const { Require(value != nullptr, "root expired"); return value; }
            void Reset() { value = nullptr; }
            explicit operator bool() const { return value != nullptr; }
        };
        struct Arena { void* Keep(void* value) { return value; } };
        inline void* TypeObject(void* type, Arena&) { return type; }
        inline bool Alive(void* object) { return object && O(object)->alive; }
        inline std::vector<void*> Items(void* array, size_t = 16384) { return O(array)->items; }
        template<class T> T Read(const void* method, void* target = nullptr)
        { void* boxed = Call(method, target); T value; std::memcpy(&value, O(boxed)->bytes.data(), sizeof(T)); return value; }
        template<class T> void Set(const void* method, void* target, const T& value)
        { void* args[]{const_cast<T*>(&value)}; Call(method, target, args); }
        inline void* FieldObject(void* object, void*, Arena&) { return O(object)->director; }
        inline void FieldObjectSet(void* object, void* field, void* value)
        {
            auto target = O(object); ++mutations;
            if (field == &parentField || field == &trackParentField) target->parent = O(value);
            if (field == &directorField) target->director = O(value);
            if (field == &receiverField) target->receiver = O(value);
            if (field == &controllerField) target->clipController = O(value);
            if (field == &clipTrackField) target->clipTrack = O(value);
        }
    }
    using namespace NativeFixture;
    inline void *timelineClass = &classes[Asset], *scriptableClass = &classes[Asset], *equipmentTrackClass = &classes[EquipmentTrack],
        *bpmClass = &classes[BpmTrack], *equipmentClipClass = &classes[EquipmentClip], *bpmClipClass = &classes[BpmClip],
        *groupClass = &classes[Group], *equipmentClass = &classes[Equipment], *receiverClass = &classes[Receiver], *directorClass = &classes[Director];
    inline const void *createAsset = M(Create), *cloneObject = M(Clone), *flattenedTracks = M(Tracks), *trackClips = M(Clips),
        *clearChildren = M(ClearChildren), *clearMarkers = M(ClearMarkers), *setMuted = M(SetMuted), *mutedHierarchy = M(Muted),
        *clipAsset = M(ClipAsset), *setClipAsset = M(SetClipAsset), *clipUseBpm = M(UseBpm), *trackParent = M(Parent), *addTrack = M(AddTrack), *addChild = M(AddChild),
        *getAsset = M(GetAsset), *directorDuration = M(Duration), *equipmentReceiver = M(EquipmentReceiver), *setFixedDuration = M(SetFixed), *setDurationMode = M(SetDurationMode),
        *setAutoPlay = M(SetAuto), *setManual = M(SetManual), *setWrap = M(SetWrap), *setAsset = M(SetAsset), *equipmentInit = M(Init), *equipmentBind = M(Bind),
        *directorPlay = M(Play), *validGraph = M(Valid), *directorStop = M(Stop), *equipmentDispose = M(Dispose), *receiverClear = M(ClearReceiver),
        *directorTime = M(Time), *directorEvaluate = M(Evaluate), *equipmentUpdate = M(Update), *equipmentLate = M(Late), *receiverUpdate = M(ReceiverUpdate);
    inline void *clipParentField = &parentField, *trackParentField = &NativeFixture::trackParentField,
        *equipmentDirectorField = &directorField, *equipmentReceiverField = &receiverField,
        *clipControllerField = &controllerField, *clipTrackField = &NativeFixture::clipTrackField;
    inline int manualMode{3}, fixedMode{1}, holdMode{1};
    struct ComponentId { int component{}, go{}, scene{}; bool operator==(const ComponentId&) const = default; };
    inline ComponentId Identity(void* value, r::Arena&) { auto object = O(value); return {object->id, object->go->id, object->go->scene}; }
    inline std::vector<void*> Components(void* type, int scene, r::Arena&)
    {
        std::vector<void*> result;
        for (const auto& object : objects) if (object->alive && object->go && object->go->scene == scene && Is(object.get(), type)) result.push_back(object.get());
        return result;
    }
    inline void* NewGo(const char*, int scene, r::Arena&) { auto go = New(Go); go->scene = scene; go->active = false; return go; }
    inline void NewGo(r::Root& owned, const char* name, int scene, r::Arena& roots) { owned = r::Root(NewGo(name, scene, roots)); }
    inline void* NewComponent(void* go, void* klass, r::Arena&) { auto component = New(static_cast<Kind>(static_cast<Class*>(klass)->type)); component->go = O(go); return component; }
    inline void Destroy(r::Root& root) { if (root) { O(root.Get())->alive = false; ++destroyed; root.Reset(); } }
    struct NativeSnapshot
    {
        void Capture(int) { ++captures; calls.push_back("capture"); }
        bool Restore() { ++restores; calls.push_back("restore"); return restoreReady; }
    };
}
