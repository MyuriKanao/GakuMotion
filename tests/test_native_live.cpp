// Synthetic contracts only. No game files, models, motions or captures.
#include "GkStageLoadSettings.hpp"
#include "GkStageManager.hpp"
#include <cstdio>
#include <functional>
using namespace GakuMotion;
using namespace GakuMotion::native_live;
namespace
{
    int passed{}, failed{}, begins{}, releases{}, samples{}, polls{};
    bool beginOk{true}, releaseOk{true}, sampleOk{true};
    Operation result{Operation::Pending};
    double sampled{};
    void Check(bool ok, const char* message)
    { if (ok) ++passed; else { ++failed; std::printf("FAIL: %s\n", message); } }
    Song SongFixture(std::string id = "music-fixture", int grade = 1)
    { return {id + "@" + std::to_string(grade), "Synthetic song", "env_3d_fixture", "tln_live_fixture", grade}; }
    Request Fixture()
    { return {SongFixture(), {true, 0x1234, 17, true, injection::OwnerRole::Photography, false}, 22}; }
    Session MakeSession()
    {
        begins = releases = samples = polls = 0; beginOk = releaseOk = sampleOk = true; result = Operation::Pending;
        Session session;
        session.api = {
            [](const Request&) { ++begins; return beginOk; },
            []() { ++polls; return result; },
            [](double t) { ++samples; sampled = t; return sampleOk; },
            []() { ++releases; return releaseOk; }};
        return session;
    }
    void TestCatalog()
    {
        Catalog data;
        Check(data.Publish({SongFixture(), SongFixture("music-fixture2", 2)}), "verified pairs publish");
        Check(data.Snapshot().size() == 2 && data.Find("music-fixture@1")->stage == "env_3d_fixture", "song/grade resolves paired stage");
        Check(!data.Find("Synthetic song") && !data.HasStage("env_3d_fixture_extra"), "no title or stage prefix guesses");
        const auto stable = data.Snapshot();
        Check(!data.Publish({SongFixture(), SongFixture()}) && data.Snapshot() == stable, "duplicate identity preserves last valid catalog");
        auto bad = SongFixture(); bad.timeline = "tln_live_../../other";
        Check(!data.Publish({bad}) && data.Snapshot() == stable, "path-like resource names rejected atomically");
        Check(!data.Publish({}) && data.Snapshot() == stable, "not-ready refresh retains valid catalog");
        for (const auto& key : {"", "music-fixture", "music-fixture@12", "other@1", "music-../other@1"})
            Check(!SongKey(key), "malformed song keys rejected");
        Check(SongKey("music-fixture@1"), "synthetic song key syntax accepted");
        std::set<std::string> names{"env_3d_fixture", "tln_live_fixture"};
        Check(BundleName("fixture", "env_3d_", names) == "env_3d_fixture", "prefix normalization requires catalog membership");
        Check(!BundleName("unknown", "env_3d_", names), "uncataloged resource is not guessed");
        names.insert("env_3d_env_3d_fixture");
        Check(!BundleName("env_3d_fixture", "env_3d_", names), "ambiguous names rejected");
        auto preferred=SongFixture("music-preferred",2);preferred.variant="TrueEnd";
        Check(data.Publish({SongFixture(),preferred}) && data.ForStage("env_3d_fixture")->key==preferred.key,
            "automatic template selection prefers TrueEnd on the exact current stage");
        Check(!data.ForStage("env_3d_fixture_extra"),"automatic matching never borrows another stage's hardware by prefix");
    }
    void TestSession()
    {
        auto session = MakeSession(); auto request = Fixture();
        Check(session.Update(request, 3) && session.phase == Phase::Loading && begins == 1 && samples == 0, "pending load never samples");
        Check(session.Update(request, 9) && begins == 1 && samples == 0, "polling never starts another load");
        result = Operation::Ready;
        Check(session.Update(request, 12.5) && session.phase == Phase::Ready && sampled == 12.5, "first frame uses latest player absolute time");
        Check(session.Update(request, 12.5) && sampled == 12.5, "pause freezes sampled time");
        Check(session.Update(request, 4) && sampled == 4, "backward seek does not accumulate delta time");
        Check(session.Update(request, 45) && sampled == 45, "forward seek follows external player");
        Check(session.Update(request, NAN) && sampled == 0, "non-finite player time sanitized");
        auto other = request; ++other.owner.presenterIdentity;
        releaseOk = false;
        Check(!session.Update(other, 8) && session.phase == Phase::Releasing && begins == 1, "new owner waits for restoration");
        releaseOk = true;
        Check(!session.Update(other, 8) && !session.Held(), "old owner releases before new loading");
        Check(session.Update(other, 8) && begins == 2, "new owner may start after cleanup");
        Check(session.Update(std::nullopt, 8) && !session.Held(), "permission/capture/stop cancels ownership");
        session = MakeSession(); result = Operation::Failed;
        Check(!session.Update(request, 0) && !session.Held(), "load failure releases all held ownership");
        Check(!session.Update(request, 0) && begins == 1 && session.phase == Phase::Fault, "failure latched, no per-frame load storm");
        session.Retry(); result = Operation::Ready;
        Check(session.Update(request, 0) && begins == 2, "explicit retry starts fresh");
        sampleOk = false; releaseOk = false;
        Check(!session.Update(request, 2) && session.Held() && session.phase == Phase::Releasing, "partial frame failure retains release work");
        releaseOk = true;
        Check(!session.Update(request, 2) && !session.Held(), "frame failure restoration retried");
        Check(!session.Update(request, 2) && begins == 2, "frame failure remains latched");
        session = MakeSession(); beginOk = false;
        Check(!session.Update(request, 0) && releases == 1, "partial begin failure enters cleanup");
        auto denied = request; denied.owner.role = injection::OwnerRole::Other;
        Check(session.Update(denied, 0) && begins == 1, "non-photography request never loads");
    }
    void TestSettings()
    {
        Check(SampleTime(10, -2, 60) == 8 && SampleTime(100, 0, 60) == 60, "offset applied then clamped to song duration");
        Check(SampleTime(-1, 0, 60) == 0 && SampleTime(NAN, 0, 60) == 0 && SampleTime(4, 0, NAN) == 0, "invalid clocks stay bounded");
        Check(catalog.Publish({SongFixture()}), "runtime synthetic catalog installed");
        StageManager manager; manager.settings.loadReplacementKey = stage_load::stages[0].key;
        const auto fixed = manager.settings.loadReplacementKey;
        manager.settings.liveLighting.mode = stage_light::Mode::SongTimeline;
        manager.settings.liveLighting.songKey = "music-fixture@1";
        Check(manager.GetLoadReplacementKey() == "env_3d_fixture", "native mode selects its song's stage");
        Check(manager.settings.loadReplacementKey == fixed, "effective native stage never overwrites fixed stage preference");
        Check(stage_load::IsTarget("env_3d_fixture") && !stage_load::IsSavedTarget("env_3d_fixture"), "dynamic admission separated from restart-safe preference");
        manager.settings.liveLighting.songKey = "music-notfound@1";
        Check(manager.GetLoadReplacementKey() == fixed, "unknown song uses saved next-load preference without guessing");
        manager.settings.liveLighting.songKey.clear();
        Check(manager.GetLoadReplacementKey()==fixed,"automatic template matching preserves the selected next-load stage");
        auto settings = stage_light::Settings{}; settings.mode = stage_light::Mode::SongTimeline; settings.songKey = "music-fixture@1";
        settings.nativeOffset = 2.5f;
        const auto dir = std::filesystem::temp_directory_path() / ("gk-native-live-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        const auto path = MotionPathToUtf8(dir / "stage.json"); std::string error;
        stage_load::Settings saved{fixed, settings}, loaded;
        Check(saved.Save(path, error) && loaded.Load(path, error) && loaded.lighting == settings, "native mode, song and offset persist");
        saved.lighting.nativeOffset = INFINITY;
        Check(!saved.Save(path, error) && loaded.Load(path, error) && loaded.lighting == settings, "invalid offset preserves prior bytes");
        std::filesystem::remove(dir / "stage.json"); std::filesystem::remove(dir);
    }
    void TestLayerSettings()
    {
        const stage_light::Settings defaults;
        Check(defaults.environment && defaults.monitor && defaults.audience, "native layers default to enabled");
        const auto dir = std::filesystem::temp_directory_path() /
            ("gk-native-layers-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        const auto file = dir / "stage.json";
        const auto path = MotionPathToUtf8(file);
        stage_load::Settings saved, loaded;
        saved.lighting.mode = stage_light::Mode::SongTimeline;
        std::string error;
        for (int mask = 0; mask < 8; ++mask)
        {
            saved.lighting.environment = (mask & 1) != 0;
            saved.lighting.monitor = (mask & 2) != 0;
            saved.lighting.audience = (mask & 4) != 0;
            Check(saved.Save(path, error) && loaded.Load(path, error) && loaded.lighting == saved.lighting,
                "each independent native layer combination round-trips in version-1 settings");
        }
        const auto write = [&](const std::string& extra) {
            std::ofstream out(file);
            out << "{\"version\":1,\"load_replacement_key\":\"\",\"live_lighting\":{"
                "\"mode\":4,\"palette\":0,\"strength\":1,\"bpm\":120,\"speed\":1" << extra << "}}";
        };
        write("");
        Check(loaded.Load(path, error) && loaded.lighting.environment && loaded.lighting.monitor && loaded.lighting.audience,
            "older live_lighting objects default missing layer options to enabled");
        write(",\"monitor\":false");
        Check(loaded.Load(path, error) && !loaded.lighting.monitor && loaded.lighting.environment && loaded.lighting.audience,
            "partial optional layer configuration preserves other defaults");
        const auto stable = loaded.lighting;
        for (const char* name : {"environment", "monitor", "audience"})
            for (const char* value : {"0", "\"true\"", "null", "[]"})
            {
                write(std::string(",\"") + name + "\":" + value);
                Check(!loaded.Load(path, error) && loaded.lighting == stable,
                    "invalid native layer type is rejected without partially changing desired settings");
            }
        std::filesystem::remove(file); std::filesystem::remove(dir / "stage.json.bak"); std::filesystem::remove(dir);
    }
    void TestLayerSessionIdentity()
    {
        for (auto member : {&Request::environment, &Request::monitor, &Request::audience})
        {
            auto session = MakeSession(); result = Operation::Ready;
            auto current = Fixture(), desired = current;
            Check(session.Update(current, 1) && begins == 1, "native layer fixture starts one owned session");
            desired.*member = false;
            Check(!current.Same(desired), "each layer choice participates in native session identity");
            releaseOk = false;
            Check(!session.Update(desired, 2) && session.Held() && begins == 1 && samples == 1,
                "changing a layer waits for prior scene cleanup before evaluating or loading another composition");
            releaseOk = true;
            Check(!session.Update(desired, 3) && !session.Held() && begins == 1,
                "layer transition fully releases the old session before rebuilding");
            Check(session.Update(desired, 4) && begins == 2 && sampled == 4,
                "layer transition rebuilds at the latest absolute time after cleanup");
            Check(session.Update(desired, 5) && begins == 2, "unchanged native layer choices do not reload every frame");
        }
    }
}
int main()
{
    TestCatalog(); TestSession(); TestSettings(); TestLayerSettings(); TestLayerSessionIdentity();
    std::printf("Native Live synthetic contracts: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
