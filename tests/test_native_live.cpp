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
}
int main()
{
    TestCatalog(); TestSession(); TestSettings();
    std::printf("Native Live synthetic contracts: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
