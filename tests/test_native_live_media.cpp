// Production media callback guard exercised through synthetic metadata/hook boundaries.
// No game, user media, network, or external Unity process is accessed.
#include <cstdio>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>

using GakuStatus = int;
inline constexpr GakuStatus GAKU_OK = 0;
struct GakuHookRequest { void* target; void* replacement; void** original; };

namespace MediaFixture
{
    using Callback = void (*)(void*, const void*);
    struct Go { int scene{}; };
    struct Component { Go* go{}; };
    struct Method { void* address{}; };
    inline bool mainThread{true}, classAvailable{true}, methodAvailable{true}, writeOriginal{true}, readFails{};
    inline int acquireStatus{}, releaseStatus{}, acquisitions{}, releases{}, forwards{}, invocations{}, pins{};
    inline int componentToken{}, sceneToken{}, sceneNameToken{}, classToken{}, hookContext{}, boxedScene{};
    inline void *lastValue{}, *releasedTarget{};
    inline const void* lastMethod{};
    inline Callback replacement{};
    inline std::map<int, std::string> sceneNames;
    inline void Original(void* value, const void* method) { ++forwards; lastValue = value; lastMethod = method; }
    inline Method onEnable{reinterpret_cast<void*>(Original)};
    inline void* Call(const void* method, void* target, void** args)
    {
        ++invocations;
        if (!mainThread || readFails) throw std::runtime_error("synthetic IL2CPP access unavailable");
        if (method == &componentToken)
        {
            if (!target || !static_cast<Component*>(target)->go) throw std::runtime_error("missing component GO");
            return static_cast<Component*>(target)->go;
        }
        if (method == &sceneToken)
        { boxedScene = static_cast<Go*>(target)->scene; return &boxedScene; }
        if (method == &sceneNameToken)
            return &sceneNames.at(*static_cast<int*>(args[0]));
        throw std::runtime_error("unexpected metadata token");
    }
    inline GakuStatus Acquire(void* context, unsigned count, const GakuHookRequest* requests)
    {
        if (context != &hookContext || count != 1 || requests[0].target != onEnable.address)
            throw std::runtime_error("bad synthetic hook request");
        ++acquisitions;
        replacement = reinterpret_cast<Callback>(requests[0].replacement);
        if (writeOriginal) *requests[0].original = reinterpret_cast<void*>(Original);
        return acquireStatus;
    }
    inline GakuStatus Release(void* context, void* target)
    {
        if (context != &hookContext) throw std::runtime_error("bad synthetic release context");
        ++releases; releasedTarget = target; return releaseStatus;
    }
}

namespace GakuMotion::host
{
    inline struct
    {
        void* context{&MediaFixture::hookContext};
        GakuStatus (*acquireBatch)(void*, unsigned, const GakuHookRequest*){MediaFixture::Acquire};
        GakuStatus (*release)(void*, void*){MediaFixture::Release};
    } hooks;
}
inline bool ProtectionOnUnityThread() { return MediaFixture::mainThread; }
namespace StageLoader
{
    inline const void *componentGo = &MediaFixture::componentToken, *goScene = &MediaFixture::sceneToken,
        *sceneName = &MediaFixture::sceneNameToken;
}
namespace NativeLiveRuntime::r
{
    inline void Require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
    struct Type { int code{}; };
    struct Arena
    {
        int kept{};
        void* Keep(void* value) { if (value) { ++kept; ++MediaFixture::pins; } return value; }
        ~Arena() { MediaFixture::pins -= kept; }
    };
    inline void* Class(const char* assembly, const char* ns, const char* name)
    {
        Require(std::string(assembly) == "campus-submodule.Runtime.dll" && std::string(ns) == "Campus.Timeline" &&
            std::string(name) == "CampusTimelineCriVideoPlayer", "unexpected synthetic class lookup");
        return MediaFixture::classAvailable ? &MediaFixture::classToken : nullptr;
    }
    inline const void* Find(void* owner, const char* name, bool isStatic, Type result, std::initializer_list<Type> args)
    {
        Require(std::string(name) == "OnEnable" && !isStatic && result.code == 0x01 && args.size() == 0,
            "unexpected synthetic lifecycle signature");
        return owner && MediaFixture::methodAvailable ? &MediaFixture::onEnable : nullptr;
    }
    inline void* Call(const void* method, void* target = nullptr, void** args = nullptr)
    { return MediaFixture::Call(method, target, args); }
    template<class T> T Read(const void* method, void* target)
    { static_assert(std::is_same_v<T, int>); return *static_cast<T*>(Call(method, target)); }
    inline std::string Text(void* value, size_t limit)
    {
        const auto& text = *static_cast<std::string*>(value);
        Require(text.size() <= limit, "synthetic text bound exceeded"); return text;
    }
}
#include "GkNativeLiveMediaGate.hpp"

namespace
{
    namespace gate = NativeLiveRuntime::media_gate;
    using namespace MediaFixture;
    int passed{}, failed{};
    void Check(bool ok, const char* label)
    { if (ok) ++passed; else { ++failed; std::printf("FAIL: %s\n", label); } }
    template<class F> bool Throws(F action)
    { try { action(); return false; } catch (const std::exception&) { return true; } }
    void Reset()
    {
        gate::Disarm(); gate::lease = nullptr; gate::ready = false; gate::original = nullptr; gate::blocked = 0;
        mainThread = classAvailable = methodAvailable = writeOriginal = true; readFails = false;
        acquireStatus = releaseStatus = acquisitions = releases = forwards = invocations = pins = 0;
        replacement = nullptr; lastValue = releasedTarget = nullptr; lastMethod = nullptr;
        onEnable.address = reinterpret_cast<void*>(Original);
        GakuMotion::host::hooks.acquireBatch = Acquire; GakuMotion::host::hooks.release = Release;
        sceneNames = {{11, "source_fixture"}, {33, "source_fixture"}, {44, "other_scene"}, {55, "source_fixture_extra"}};
    }
    void TestUnavailableHook()
    {
        for (int reason = 0; reason < 4; ++reason)
        {
            Reset();
            if (reason == 0) classAvailable = false;
            if (reason == 1) methodAvailable = false;
            if (reason == 2) onEnable.address = nullptr;
            if (reason == 3) GakuMotion::host::hooks.acquireBatch = nullptr;
            Check(Throws([] { gate::Arm("source_fixture", {11}); }), "missing metadata/address/hook fails before arming");
            Check(gate::expectedName.empty() && !gate::lease && !gate::ready && acquisitions == 0,
                "unavailable hook publishes no receipt or lease");
        }
        Reset(); mainThread = false;
        Check(Throws([] { gate::Arm("source_fixture", {11}); }) && acquisitions == 0,
            "worker callback cannot install or arm a Unity lifecycle hook");
    }
    void TestCallbackScope()
    {
        Reset(); gate::Arm("source_fixture", {11});
        Check(gate::ready && gate::lease == onEnable.address && replacement && acquisitions == 1,
            "exact production OnEnable hook is acquired before source receipt is armed");
        Go ownedGo{33}, oldGo{11}, foreignGo{44}, prefixGo{55}, missingGo{99}, noScene{};
        Component owned{&ownedGo}, old{&oldGo}, foreign{&foreignGo}, prefix{&prefixGo}, missing{&missingGo}, none{&noScene};
        int callMethod{};
        replacement(&owned, &callMethod);
        Check(forwards == 0 && gate::blocked == 1 && pins == 0, "owned fresh exact source suppresses movie setup and releases roots");
        for (auto* component : {&old, &foreign, &prefix, &none}) replacement(component, &callMethod);
        Check(forwards == 4 && gate::blocked == 1 && lastValue == &none && lastMethod == &callMethod,
            "preexisting, foreign, prefix-matched and zero scenes preserve original callback arguments");
        const int before = invocations; mainThread = false; replacement(&owned, &callMethod); mainThread = true;
        Check(forwards == 5 && gate::blocked == 1 && invocations == before, "worker callback forwards without invoking IL2CPP reflection");
        replacement(&missing, &callMethod);
        readFails = true; replacement(&owned, &callMethod); readFails = false;
        replacement(nullptr, &callMethod);
        Check(forwards == 8 && gate::blocked == 1 && pins == 0, "unproven or failed identity reads forward instead of suppressing another component");
        Check(Throws([] { gate::Arm("other_scene", {}); }) && gate::expectedName == "source_fixture" && acquisitions == 1,
            "another source cannot replace a pending owned source receipt");
        gate::Release();
        Check(releases == 0 && gate::lease && gate::ready, "pending source load/unload retains its guard lease until disarmed");
        gate::Disarm(); replacement(&owned, &callMethod);
        Check(forwards == 9 && gate::expectedName.empty() && gate::preexisting.empty(), "disarming restores normal callbacks immediately");
        gate::Release();
        Check(releases == 1 && releasedTarget == onEnable.address && !gate::lease && !gate::ready,
            "disarmed source releases exactly the acquired target");
    }
    void TestAbnormalLease()
    {
        for (int abnormal = 0; abnormal < 2; ++abnormal)
        {
            Reset(); acquireStatus = abnormal == 0 ? -1 : GAKU_OK; writeOriginal = abnormal == 0;
            Check(Throws([] { gate::Arm("source_fixture", {}); }), "incomplete or abnormal hook result rejects arming");
            Check(gate::lease == onEnable.address && !gate::ready && gate::expectedName.empty(),
                "returned trampoline or successful lease retains its cleanup identity without publishing readiness");
            Check(Throws([] { gate::Arm("source_fixture", {}); }) && acquisitions == 1,
                "unready held lease is not silently treated as ready or acquired repeatedly");
            releaseStatus = -1; gate::Release();
            Check(gate::lease && !gate::ready && releases == 1, "failed host release retains the abnormal lease for retry");
            releaseStatus = GAKU_OK; gate::Release();
            Check(!gate::lease && !gate::ready && releases == 2, "abnormal lease identity clears only after successful release");
            acquireStatus = GAKU_OK; writeOriginal = true; gate::Arm("source_fixture", {});
            Check(gate::ready && acquisitions == 2, "a new source may arm after abnormal cleanup succeeds");
            gate::Disarm(); gate::Release();
        }
    }
    void TestRearm()
    {
        Reset(); gate::Arm("source_fixture", {11});
        Go source{33}; Component value{&source}; replacement(&value, nullptr);
        gate::Disarm(); gate::Arm("other_scene", {11, 33});
        Check(acquisitions == 1 && gate::blocked == 0, "sequential receipts reuse the installed lease and reset counters");
        replacement(&value, nullptr);
        source.scene = 44; replacement(&value, nullptr);
        Check(forwards == 1 && gate::blocked == 1, "rearming changes exact source identity without blocking the preceding source");
        gate::Disarm(); gate::Release(); gate::Release();
        Check(releases == 1, "repeated successful release is idempotent");
        gate::Arm("source_fixture", {11}); source.scene = 33; replacement(&value, nullptr);
        Check(acquisitions == 2 && gate::ready && gate::blocked == 1 && forwards == 1,
            "rearming after full hook release reacquires and suppresses only the new receipt");
        gate::Disarm(); gate::Release();
    }
}
int main()
{
    TestUnavailableHook(); TestCallbackScope(); TestAbnormalLease(); TestRearm();
    std::printf("Native Live media lifecycle guard: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
