// Synthetic fixtures only: no game, recorded motion, model or capture input.
#include "GkStageManager.hpp"
#include "GkStageLoadSettings.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>

namespace
{
    using namespace GakuMotion;
    using namespace stage_light;
    int passed{}, failed{}, reads{}, writes{}, ends{}, failWrites{};
    bool enumerationReady{true}, readReady{true};
    struct Fixture { Identity id; Values value; bool active{true}; };
    std::vector<std::unique_ptr<Fixture>> fixtures;
    void Check(bool ok, const char* description)
    { if (ok) ++passed; else { ++failed; std::printf("[FAIL] %s\n", description); } }
    bool Begin(std::vector<Target>& targets)
    {
        if (!enumerationReady) return false;
        for (const auto& fixture : fixtures) targets.push_back({fixture->id, fixture.get(), fixture->active});
        return true;
    }
    bool Read(const Target& target, Values& value)
    { ++reads; value = static_cast<Fixture*>(target.token)->value; return readReady; }
    bool Write(const Target& target, const Values& value)
    {
        ++writes;
        auto& actual = static_cast<Fixture*>(target.token)->value;
        actual.color = value.color; // Model a setter that mutates before failing.
        if (failWrites) { if (failWrites > 0) --failWrites; return false; }
        actual = value;
        return true;
    }
    Controller MakeController()
    {
        fixtures.clear(); reads = writes = ends = failWrites = 0;
        enumerationReady = readReady = true;
        Controller result;
        result.api = {Begin, [] { ++ends; }, Read, Write};
        return result;
    }
    Values Baseline()
    { return {{0.8f, 0.7f, 0.6f, 1}, 2.0f, 1.5f, false, true, true, 1}; }
    Fixture& Add(Kind kind, int component, int scene = 17)
    {
        fixtures.push_back(std::make_unique<Fixture>(Fixture{{kind, component, component + 100, scene}, Baseline()}));
        return *fixtures.back();
    }
    void TestNativeBaseline()
    {
        auto controller = MakeController();
        auto& light = Add(Kind::Light, 51);
        auto& inactive = Add(Kind::Beam, 52); inactive.active = false;
        auto& foreign = Add(Kind::Light, 53, 99);
        Check(controller.Capture(17) && writes == 0 && controller.HasChanges(), "native baseline capture never writes and includes inactive stage devices");
        light.value.intensity = 7; inactive.value.colorMode = 0; foreign.value.intensity = 5;
        failWrites = 1;
        Check(!controller.Restore() && controller.HasChanges(), "native baseline survives a partial restoration failure");
        Check(controller.Restore() && light.value == Baseline() && inactive.value == Baseline() && foreign.value.intensity == 5,
            "native baseline restores only the exact paired stage");
        const int before = writes;
        Check(controller.Update({Mode::SongTimeline}, 12, 17) && writes == before, "procedural adapter never synthesizes cues for native song mode");
    }
    void TestSampler()
    {
        Settings settings{Mode::Cycle, 0, 1.25f, 128, 0.75f};
        const auto first = Sample(settings, 1.2, .25);
        const auto next = Sample(settings, 3.6, .25);
        Check(first.color != next.color, "color cycle changes the visible fixture color");
        Check(first.color == Sample(settings, 1.2, .25).color && first.intensity == Sample(settings, 1.2, .25).intensity,
            "seek/pause deterministically resamples absolute performance time");
        settings.mode = Mode::Wash;
        Check(Sample(settings, 0, .25).color == Sample(settings, 20, .25).color, "wash is stationary");
        const auto invalid = Sanitize({static_cast<Mode>(100), 9, NAN, INFINITY, -1});
        Check(invalid.mode == Mode::Native && invalid.palette == 2 && invalid.strength == 1 && invalid.bpm == 120 && invalid.speed == .1f,
            "invalid mode and non-finite parameters are bounded");
        for (double time : std::array<double, 5>{-1.0, 0.0, 1e300,
            std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            const auto cue = Sample({Mode::Cycle}, time, NAN);
            Check(std::isfinite(cue.intensity) && cue.intensity >= .4f && cue.intensity <= 1 &&
                std::isfinite(cue.color[0]), "extreme time and phase keep a finite, non-strobing cue");
        }
        Check(std::isfinite(ScaleIntensity((std::numeric_limits<float>::max)(), 3)), "intensity multiplication cannot overflow");
    }
    void TestOwnership()
    {
        auto controller = MakeController();
        auto& light = Add(Kind::Light, 1);
        auto& beam = Add(Kind::Beam, 2);
        auto& foreign = Add(Kind::Light, 3, 99);
        auto& inactive = Add(Kind::Beam, 4); inactive.active = false;
        const auto original = Baseline();
        Check(controller.Update({Mode::Cycle}, 1, 17) && controller.lights == 1 && controller.beams == 1,
            "only active fixtures in the exact replacement scene are driven");
        Check(light.value != original && beam.value.colorMode == 0 && !beam.value.colorFromLight &&
            !beam.value.intensityFromLight && beam.value.enabled, "real light and volumetric beam paths receive cues");
        Check(foreign.value == original && inactive.value == original, "other scenes and inactive fixtures are untouched");
        Check(controller.Restore() && light.value == original && beam.value == original && !controller.HasChanges(),
            "release restores every original field, including disabled/gradient/from-light flags");
        Check(ends == 2, "enumeration roots are released after update and restoration");
        controller.Update({Mode::Wash}, 0, 17);
        fixtures[0] = std::make_unique<Fixture>(*fixtures[0]); // Relocate the managed token, preserving identity.
        Check(controller.Restore() && fixtures[0]->value == original, "restoration uses fresh tokens rather than cached pointers");
        controller.Update({Mode::Wash}, 0, 17);
        fixtures[0]->id.gameObject += 1000;
        fixtures[0]->value = original;
        fixtures.erase(fixtures.begin() + 1); // Old beam was destroyed.
        const int before = writes;
        Check(controller.Restore() && !controller.HasChanges() && writes == before,
            "destroyed/reused component identity is retired without touching its replacement");
    }
    void TestFailuresAndTransitions()
    {
        auto controller = MakeController();
        auto& light = Add(Kind::Light, 10);
        const auto original = light.value;
        failWrites = -1;
        Check(!controller.Update({Mode::Cycle}, 4, 17) && controller.HasChanges(), "partial setter failure retains restoration baseline");
        enumerationReady = false;
        Check(!controller.Restore() && controller.HasChanges(), "failed enumeration never discards pending restoration");
        enumerationReady = true; failWrites = 0;
        Check(!controller.Update({Mode::Wash}, 5, 17) && !controller.HasChanges() && light.value == original,
            "retry cleans up before permitting another performance mutation");
        controller.Update({Mode::Cycle}, 1, 17);
        auto& nextScene = Add(Kind::Beam, 11, 18);
        Check(controller.Update({Mode::Wash}, 2, 18) && light.value == original && nextScene.value != original,
            "scene transition restores old still-loaded stage before driving a new one");
        Check(controller.Update({Mode::Native}, 0, 18) && nextScene.value == original, "native mode immediately restores originals");
        controller.Update({Mode::Wash}, 0, 18);
        Check(controller.Update({Mode::Cycle}, 0, 0) && !controller.HasChanges(), "loss of verified scene revokes lighting");
        readReady = false;
        const int before = writes;
        Check(!controller.Update({Mode::Wash}, 0, 17) && writes == before, "failed baseline read prevents the first write");
    }
    bool restoreReady = true;
    int restores{}, updates{};
    double observedTime{};
    Mode observedMode{};
    bool RestoreBridge() { ++restores; return restoreReady; }
    bool UpdateBridge(const Settings& settings, double time, std::string&)
    { ++updates; observedTime = time; observedMode = settings.mode; return true; }
    void TestManagerLifecycle()
    {
        StageManager manager;
        manager.api.fnRestoreStageLighting = RestoreBridge;
        manager.api.fnUpdateStageLighting = UpdateBridge;
        manager.settings.liveLighting.mode = Mode::Cycle;
        Check(manager.HasCustomizations(), "custom lighting participates in stage ownership/capture protection");
        manager.SetInjectionEnabled(true);
        manager.OnLateUpdate(nullptr, .1f, "Photography", nullptr, 12.5);
        Check(updates == 1 && observedTime == 12.5 && observedMode == Mode::Cycle, "manager passes playback time to the bridge");
        manager.settings.lightingEffectsEnabled = false;
        manager.OnLateUpdate(nullptr, .1f, "Photography", nullptr, 12.5);
        Check(observedMode == Mode::Native, "global lighting switch restores rather than leaving animated overrides");
        manager.settings.customNodeControl = true;
        manager.OnLateUpdate(nullptr, .1f, "Photography", nullptr, 12.5);
        Check(observedMode == Mode::Native, "manual node editing takes precedence over automatic lighting");
        restoreReady = false;
        Check(!manager.SetInjectionEnabled(false) && !manager.SetInjectionEnabled(true), "failed restoration blocks resuming ownership");
        restoreReady = true;
        Check(manager.SetInjectionEnabled(false), "capture/transport suspension retries lighting release");
        const int before = restores;
        manager.AbandonScene();
        Check(restores > before, "scene abandonment attempts fresh-identity lighting cleanup");
    }
    void TestCatalogAndSettings()
    {
        std::set<std::string> keys;
        injection::OwnerSnapshot owner{true, 0x1234, 17, true, injection::OwnerRole::Photography, false};
        for (const auto& stage : stage_load::stages)
        {
            stage_load::Receipt receipt;
            Check(keys.insert(stage.key).second && stage_load::IsTarget(stage.key), "catalog entries are unique and exactly allowlisted");
            Check(receipt.Record(owner, stage.key) && receipt.Context(stage.key, "Photography", owner) == injection::Context::Photography,
                "each new stage requires the same exact Photography owner receipt");
            auto other = owner; ++other.presenterIdentity;
            Check(receipt.Context(stage.key, "Photography", other) == injection::Context::Unknown,
                "new stages do not broaden owner admission");
        }
        Check(!stage_load::IsTarget("env_3d_live_all001-00-noon_extra") && !stage_load::IsTarget("env_3d_live_hrnm001-00-noon"),
            "unverified song keys and prefix matches stay excluded");
        const auto directory = std::filesystem::temp_directory_path() /
            ("gakumotion-stage-lighting-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
        const auto path = directory / "stage.json";
        const auto utf8 = MotionPathToUtf8(path);
        std::string error;
        stage_load::Settings saved{stage_load::stages[3].key, {Mode::Cycle, 2, 1.25f, 128, .75f}}, loaded;
        Check(saved.Save(utf8, error) && loaded.Load(utf8, error) && saved.key == loaded.key && saved.lighting == loaded.lighting,
            "expanded stage and lighting settings round-trip atomically");
        { std::ofstream out(path); out << "{\"version\":1,\"load_replacement_key\":\"env_3d_live_all001-00-noon\"}"; }
        Check(loaded.Load(utf8, error) && loaded.lighting.mode == Mode::Native, "old version-1 settings retain native lighting by default");
        saved.lighting.bpm = NAN;
        Check(!saved.Save(utf8, error), "non-finite settings are rejected without replacing saved bytes");
        Check(loaded.Load(utf8, error) && loaded.lighting.mode == Mode::Native, "failed save preserves previous file");
        { std::ofstream out(path); out << "{\"version\":1,\"load_replacement_key\":\"\",\"live_lighting\":{\"mode\":99}}"; }
        Check(!loaded.Load(utf8, error), "malformed lighting settings are rejected");
        std::filesystem::remove(path);
        std::filesystem::remove(directory / "stage.json.bak");
        std::filesystem::remove(directory);
    }
}
int main()
{
    TestNativeBaseline(); TestSampler(); TestOwnership(); TestFailuresAndTransitions(); TestManagerLifecycle(); TestCatalogAndSettings();
    std::printf("Stage lighting synthetic contracts: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
