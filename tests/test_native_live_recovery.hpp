// Included after the synthetic Session fixtures; no game input is used.
namespace
{
    struct OriginNode
    {
        std::string name;
        OriginNode* parent{};
        std::vector<void*> children;
        bool active{true};
    };
    OriginNode* originRoot{};
    void TestOriginCandidateVisibility()
    {
        OriginNode wrapper{native_live::kStageOriginRoot}, floor{"Background", &wrapper},
            lights{"Lighting&PostEffects", &wrapper}, spot{"SpotLight", &lights};
        wrapper.children = {&floor, &lights}; lights.children = {&spot}; originRoot = &wrapper;
        StageManager manager;
        manager.api.fnFindSceneRoots = [] { return std::vector<void*>{originRoot}; };
        manager.api.fnGetParent = [](void* p) -> void* { return static_cast<OriginNode*>(p)->parent; };
        manager.api.fnGetChildCount = [](void* p) { return static_cast<int>(static_cast<OriginNode*>(p)->children.size()); };
        manager.api.fnGetChild = [](void* p, int i) { return static_cast<OriginNode*>(p)->children[i]; };
        manager.api.fnGetGameObject = [](void* p) { return p; };
        manager.api.fnGetObjectName = [](void* p) { return static_cast<OriginNode*>(p)->name; };
        manager.api.fnGetActiveSelf = [](void* p) { return static_cast<OriginNode*>(p)->active; };
        manager.api.fnSetActive = [](void* p, bool active) { static_cast<OriginNode*>(p)->active = active; return true; };
        manager.DiscoverCandidates(nullptr, "Photography");
        Check(manager.candidates.size() == 3 && manager.candidates[0].depth == 0 && manager.candidates[1].isLightingRoot &&
            manager.candidates[1].depth == 0 && manager.candidates[2].isLightingNode,
            "origin wrapper is transparent to background and lighting candidate identities and depths");
        manager.SetInjectionEnabled(true); manager.ApplyBackgroundVisibility(true);
        Check(!floor.active && wrapper.active && lights.active && spot.active,
            "hiding the background does not hide the placement frame or its lighting rig");
        Check(manager.SetInjectionEnabled(false) && floor.active && wrapper.active && lights.active,
            "capture restoration retains original candidate visibility with the placement frame");
        originRoot = nullptr;
    }
    Session* restoringSession{};
    void TestActorReplacementRecovery()
    {
        auto session = MakeSession(); restoringSession = &session;
        const auto request = Fixture();
        session.Update(request, 0);
        StageManager manager;
        manager.api.fnRestoreStageLighting = [] { return restoringSession->Release(); };
        manager.api.fnGetParent = [](void*) -> void* { return nullptr; };
        manager.api.fnGetChildCount = [](void*) { return 0; };
        manager.api.fnGetChild = [](void*, int) -> void* { return nullptr; };
        manager.api.fnGetGameObject = [](void*) -> void* { return nullptr; };
        manager.api.fnGetObjectName = [](void*) -> std::string { return {}; };
        manager.api.fnBeginAccess = [] { return false; };
        manager.settings.liveLighting.mode = stage_light::Mode::SongTimeline;
        manager.settings.liveLighting.songKey = request.song.key;
        manager.mountedStageKey = "synthetic-mounted-stage";
        manager.api.fnSetMountedStageActive = [](bool) { return true; };
        manager.candidates.emplace_back(); manager.candidates[0].instanceId = 77;
        manager.candidates[0].userMuted = true;
        manager.candidatesDiscovered = true;
        manager.SetInjectionEnabled(true);
        releaseOk = false;
        Check(!manager.OnActorSetChanged(nullptr) && !manager.injectionEnabled && !manager.suspensionRestored,
            "actor replacement keeps asynchronous release pending instead of certifying a restored stage");
        Check(!manager.candidatesDiscovered && manager.visibilityRefreshRequested && session.phase == Phase::Releasing,
            "pending actor replacement invalidates discovery and retains the native session cleanup phase");
        manager.RescanCandidates(nullptr);
        Check(manager.candidates.size() == 1 && manager.candidates[0].instanceId == 77 && manager.candidates[0].userMuted,
            "read-only rescan cannot discard restoration identities or mute choices during cleanup");
        Check(!manager.SetInjectionEnabled(true) && !manager.SetInjectionEnabled(false) && begins == 1,
            "both resuming stage ownership and taking a capture remain gated while release is pending");
        Check(!session.Update(request, 8) && begins == 1 && samples == 0,
            "pending actor cleanup cannot start a second load or sample the retired source");
        releaseOk = true;
        Check(manager.SetInjectionEnabled(true) && manager.suspensionRestored && !session.Held(),
            "ordinary frame retry resumes stage control after asynchronous cleanup completes");
        Check(manager.mountedStageKey == "synthetic-mounted-stage" && !manager.candidatesDiscovered &&
            manager.settings.liveLighting.songKey == request.song.key,
            "actor refresh preserves mounted ownership and desired template and requests fresh discovery");
        result = Operation::Ready;
        Check(session.Update(request, 9) && begins == 2 && sampled == 9,
            "recovered actor replacement can start the desired template at the latest playback time");
        session.Release(); restoringSession = nullptr;
    }
}
