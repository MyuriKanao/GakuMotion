// Pure composition/track-admission rules. Synthetic hierarchy shaped after the
// static root census of tln_live_all-001/012/019; no game files are read.
#include "GkNativeLiveComposition.hpp"
#include <algorithm>
#include <cstdio>
using namespace GakuMotion::native_live::composition;
namespace
{
    int passed{}, failed{};
    void Check(bool ok, const char* label) { if (ok) ++passed; else { ++failed; std::printf("FAIL: %s\n", label); } }
    template<class F> bool Throws(F action) { try { action(); return false; } catch (const std::exception&) { return true; } }
    enum Root { Receiver = 1, RealLight, Environment_, PostEffect, Monitor_, Crowd_, Mob_, Master, CameraRoot, Audio, Particles, Overlay, Deco };
    std::vector<Node> Live()
    {
        std::vector<Node> n{
            {Receiver, 0, Rig, false, false, true}, {101, Receiver, Rig},
            {RealLight, 0, Rig}, {Environment_, 0, Environment}, {PostEffect, 0, Environment},
            {Monitor_, 0, Monitor, false, true}, {501, Monitor_, Monitor},
            {Crowd_, 0, Audience}, {Mob_, 0, Audience | Environment},
            {Master, 0, 0, true}, {801, Master, 0}, {CameraRoot, 0, 0, true}, {Audio, 0, 0, true},
            {Particles, 0, 0, false, false, false, true}, {Overlay, 0, Environment, false, true},
            {Deco, 0, Rig, false, false, false, true}};
        return n;
    }
    bool Has(const Result& r, int id) { return std::any_of(r.roots.begin(), r.roots.end(), [&](auto& x) { return x.id == id; }); }
    void TestPlan()
    {
        auto all = Plan(Live(), All);
        Check(Has(all, Receiver) && Has(all, RealLight) && Has(all, Environment_) && Has(all, Monitor_) && Has(all, Crowd_),
            "all layers admit receiver rig, environment, screen and audience roots");
        Check(!Has(all, Master) && !Has(all, CameraRoot) && !Has(all, Audio), "actor/camera/audio roots are never planned");
        Check(!Has(all, Particles) && Has(all, Deco), "stand-alone particles stay hidden; particles inside a light rig follow it");
        Check(!Has(all, Overlay) && all.excluded == 1, "screen-space overlay outside a screen root is excluded and counted");
        Check(all.receiver, "the active source receiver root is reported");
        auto rig = Plan(Live(), Rig);
        Check(Has(rig, Receiver) && !Has(rig, Environment_) && !Has(rig, Monitor_) && !Has(rig, Crowd_) && !Has(rig, Mob_),
            "light-only mask keeps every optional layer hidden");
        auto noAudience = Plan(Live(), Rig | Environment | Monitor);
        Check(!Has(noAudience, Mob_) && !Has(noAudience, Crowd_) && Has(noAudience, PostEffect),
            "disabled audience hides mixed crowd roots even when they also carry meshes");
        auto noScreen = Plan(Live(), Rig | Environment | Audience);
        Check(!Has(noScreen, Monitor_), "disabled screen hides the screen root and its render camera");
        auto paired = Plan(Live(), All, {}, false);
        Check(!Has(paired, Receiver) && Has(paired, RealLight) && !paired.receiver,
            "a paired stage receiver suppresses the duplicate source receiver rig");
        auto inactive = Plan(Live(), All, {{Monitor_, false}});
        const auto screen = std::find_if(inactive.roots.begin(), inactive.roots.end(), [](auto& x) { return x.id == Monitor_; });
        Check(screen != inactive.roots.end() && !screen->active, "authored inactive roots stay inactive");
        auto badReceiver = Live(); badReceiver.push_back({999, Receiver, 0, true});
        Check(Throws([&] { Plan(badReceiver, All); }), "receiver root that also holds an actor fails instead of hiding the rig");
        Check(Throws([] { Plan({{1, 2}, {2, 1}}, All); }), "hierarchy cycles are rejected");
        Check(Throws([] { Plan({{1, 0}, {1, 0}}, All); }) && Throws([] { Plan({{0, 0}}, All); }), "duplicate or zero identities rejected");
        Check(Throws([] { Plan({}, 16); }), "unknown feature bits rejected");
        Check(NameHint("Mob") == Audience && NameHint("Crowd") == Audience && NameHint("Environment") == 0, "crowd name hints are exact");
    }
    void TestTracks()
    {
        const TrackFacts volume{Track::Volume, true, true, Binding::None};
        Check(Decide(volume, All) == Environment && !Decide(volume, Rig | Monitor | Audience), "volume tracks follow the environment layer");
        const TrackFacts profile{Track::VolumeProfile, true, true, Binding::Admitted, Environment};
        Check(Decide(profile, All) == Environment, "profile track bound to an admitted environment controller is cloned");
        Check(!Decide({Track::VolumeProfile, true, true, Binding::Hidden}, All), "profile track bound to a hidden root is dropped");
        Check(Decide({Track::Light, true, true, Binding::Admitted, Rig}, All) == Rig, "light anchor tracks follow the rig");
        Check(!Decide({Track::Light, true, true, Binding::None}, All), "unbound light anchor track is dropped");
        Check(Decide({Track::Video, true, true, Binding::Admitted, Monitor}, All) == Monitor &&
            !Decide({Track::Video, true, true, Binding::Admitted, Monitor}, Rig | Environment | Audience), "screen video follows monitor layer");
        Check(!Decide({Track::TargetImage, true, true, Binding::Admitted, Environment}, All), "screen image track bound outside a screen is dropped");
        Check(Decide({Track::Crowd, true, true, Binding::Admitted, Audience}, All) == Audience &&
            !Decide({Track::Crowd, true, true, Binding::Admitted, Audience}, Rig | Environment | Monitor), "crowd follows audience layer");
        Check(Decide({Track::Animation, true, true, Binding::Admitted, Monitor}, All) == Monitor, "screen animation follows the bound root");
        Check(Decide({Track::Animation, true, true, Binding::Admitted, Rig | Environment}, All) == Rig, "rig wins for mixed rig roots");
        Check(!Decide({Track::Animation, true, false, Binding::Admitted, Rig}, All), "infinite/recorded animation clips are never cloned");
        Check(!Decide({Track::Animation, false, true, Binding::Admitted, Rig}, All), "animation with foreign clip assets is never cloned");
        Check(!Decide({Track::Animation, true, true, Binding::Foreign, Rig}, All), "bindings outside the owned source are dropped");
        Check(!Decide({Track::Animation, true, true, Binding::None}, All), "unbound root animation is dropped");
        Check(Decide({Track::Animation, true, true, Binding::None, 0, true, Rig}, All) == Rig, "override layers inherit their parent");
        Check(!Decide({Track::Animation, true, true, Binding::Admitted, Rig, true, 0}, All), "children of excluded tracks are dropped");
        Check(Decide({Track::Activation, true, true, Binding::Admitted, Environment}, All) == Environment &&
            !Decide({Track::Activation, true, true, Binding::Hidden}, All), "activation tracks only toggle admitted objects");
        Check(!Decide({Track::Unsupported, true, true, Binding::Admitted, All}, All), "actor/camera/audio/control tracks are unsupported");
        Check(Throws([] { Decide({}, 32); }), "decision rejects unknown feature bits");
    }
}
int main()
{
    TestPlan(); TestTracks();
    std::printf("Native Live composition rules: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
