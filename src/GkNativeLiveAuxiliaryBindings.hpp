#pragma once
// Exact metadata for the source-scene composition and its non-equipment
// tracks (environment volumes, light anchors, screens and audience).
namespace NativeLiveRuntime { namespace plan = GakuMotion::native_live::composition; }
namespace NativeLiveRuntime::auxiliary
{
    struct Rule { void* track{}; void* clip{}; plan::Track kind{}; };
    struct Bindings
    {
        std::array<Rule, 10> rules{};
        void *animationTrack{}, *activationTrack{}, *profile{}, *volumeComponent{};
        void *volume{}, *monitor{}, *crowd{}, *audienceVolume{}, *mob{}, *lightAnchor{}, *decal{}, *equipmentValue{};
        void *actor{}, *virtualActor{}, *virtualCamera{}, *liveController{}, *criPlayer{}, *videoPlayer{};
        void *camera{}, *canvas{}, *audio{}, *listener{}, *renderer{}, *particle{};
        const void *getBinding{}, *setBinding{}, *clipMode{}, *cameraTarget{}, *canvasMode{}, *setVideoAudio{};
        const void *audioStop{}, *setParent{}, *textureName{}, *propertyId{}, *getGlobalTexture{}, *setGlobalTexture{};
        void* profileComponents{};
        int worldCanvas{}, cameraCanvas{}, noVideoAudio{};
        bool ready{};

        void Resolve()
        {
            if (ready) return;
            auto campus = [](const char* ns, const char* name) { return r::Class("campus-submodule.Runtime.dll", ns, name); };
            auto timeline = [](const char* name) { return r::Class("Unity.Timeline.dll", "UnityEngine.Timeline", name); };
            const char* names[][3]{
                {"Campus.Timeline", "CampusLightTrack", "CampusLightAsset"},
                {"Campus.Timeline", "CampusVolumeTimelineTrack", "CampusVolumeTimelineAsset"},
                {"Campus.Timeline", "CampusVolumeProfileTimelineTrack", "CampusVolumeProfileTimelineAsset"},
                {"Campus.Crowd", "CrowdTimelineTrack", "CrowdTimelineAsset"},
                {"Campus.AudiencePenlight", "AudiencePenlightTimelineTrack", "AudiencePenlightTimelineAsset"},
                {"Campus.AudiencePenlight", "MobAudiencePenlightOverrideTrack", "MobAudiencePenlightOverrideAsset"},
                {"Campus.Timeline", "CampusVideoPlayerTrack", "CampusVideoPlayerClip"},
                {"Campus.Timeline", "CampusTargetImageTrack", "CampusTargetImageClip"}};
            const plan::Track kinds[]{plan::Track::Light, plan::Track::Volume, plan::Track::VolumeProfile, plan::Track::Crowd,
                plan::Track::Penlight, plan::Track::MobPenlight, plan::Track::Video, plan::Track::TargetImage};
            for (size_t i = 0; i < 8; ++i)
            {
                rules[i] = {campus(names[i][0], names[i][1]), campus(names[i][0], names[i][2]), kinds[i]};
                r::Require(rules[i].track && rules[i].clip, "auxiliary timeline type unavailable");
            }
            animationTrack = timeline("AnimationTrack"); activationTrack = timeline("ActivationTrack");
            // AnimationPlayableAsset also admits CampusMonitorAnimationPlayableAsset.
            rules[8] = {animationTrack, timeline("AnimationPlayableAsset"), plan::Track::Animation};
            rules[9] = {activationTrack, timeline("ActivationPlayableAsset"), plan::Track::Activation};
            const char* core = "Unity.RenderPipelines.Core.Runtime.dll";
            profile = r::Class(core, "UnityEngine.Rendering", "VolumeProfile");
            volumeComponent = r::Class(core, "UnityEngine.Rendering", "VolumeComponent");
            volume = r::Class(core, "UnityEngine.Rendering", "Volume");
            decal = r::Class("Unity.RenderPipelines.Universal.Runtime.dll", "UnityEngine.Rendering.Universal", "DecalProjector");
            monitor = campus("CanvasRT", "CanvasRenderTexture");
            crowd = campus("Campus.Crowd", "CrowdVolume");
            audienceVolume = campus("Campus.AudiencePenlight", "AudiencePenlightVolume");
            mob = campus("Campus.MobAudience", "MobAudiencePenlightController");
            lightAnchor = campus("Campus.Common", "CampusLightAnchor");
            equipmentValue = campus("Campus.Live.Equipment", "EquipmentValue");
            actor = campus("Campus.Common", "CampusActorController");
            virtualActor = campus("Campus.Common", "CampusVirtualActorController");
            liveController = campus("Campus.Common.Live", "CampusLiveTimelineController");
            criPlayer = campus("Campus.Timeline", "CampusTimelineCriVideoPlayer");
            virtualCamera = r::Class("Cinemachine.dll", "Cinemachine", "CinemachineVirtualCameraBase");
            videoPlayer = r::Class("UnityEngine.VideoModule.dll", "UnityEngine.Video", "VideoPlayer");
            camera = r::Unity("Camera"); renderer = r::Unity("Renderer");
            canvas = r::Class("UnityEngine.UIModule.dll", "UnityEngine", "Canvas");
            audio = r::Class("UnityEngine.AudioModule.dll", "UnityEngine", "AudioSource");
            listener = r::Class("UnityEngine.AudioModule.dll", "UnityEngine", "AudioListener");
            particle = r::Class("UnityEngine.ParticleSystemModule.dll", "UnityEngine", "ParticleSystem");
            void* object = r::Unity("Object"); void* texture = r::Unity("Texture"); void* shader = r::Unity("Shader");
            void* renderTexture = r::Unity("RenderTexture");
            void* renderMode = r::Class("UnityEngine.UIModule.dll", "UnityEngine", "RenderMode");
            void* audioMode = r::Class("UnityEngine.VideoModule.dll", "UnityEngine.Video", "VideoAudioOutputMode");
            r::Require(animationTrack && activationTrack && profile && volumeComponent && volume && decal && equipmentValue && monitor && crowd &&
                audienceVolume && mob && lightAnchor && actor && virtualActor && liveController && criPlayer && virtualCamera &&
                videoPlayer && camera && renderer && canvas && audio && listener && particle && object && texture && shader &&
                renderTexture && renderMode && audioMode, "native composition type unavailable");
            worldCanvas = r::Enum(renderMode, "WorldSpace"); cameraCanvas = r::Enum(renderMode, "ScreenSpaceCamera");
            noVideoAudio = r::Enum(audioMode, "None");
            getBinding = r::Find(directorClass, "GetGenericBinding", false, {0x12, object}, {{0x12, object}});
            setBinding = r::Find(directorClass, "SetGenericBinding", false, {0x01}, {{0x12, object}, {0x12, object}});
            clipMode = r::Find(animationTrack, "get_inClipMode", false, {0x02}, {});
            cameraTarget = r::Find(camera, "get_targetTexture", false, {0x12, renderTexture}, {});
            canvasMode = r::Find(canvas, "get_renderMode", false, {0x11, renderMode}, {});
            setVideoAudio = r::Find(videoPlayer, "set_audioOutputMode", false, {0x01}, {{0x11, audioMode}});
            audioStop = r::Find(audio, "Stop", false, {0x01}, {});
            setParent = r::Find(StageLoader::transformClass, "SetParent", false, {0x01}, {{0x12, StageLoader::transformClass}});
            textureName = r::Find(monitor, "GetTextureName", true, {0x0e}, {{0x08}});
            propertyId = r::Find(shader, "PropertyToID", true, {0x08}, {{0x0e}});
            getGlobalTexture = r::Find(shader, "GetGlobalTexture", true, {0x12, texture}, {{0x08}});
            setGlobalTexture = r::Find(shader, "SetGlobalTexture", true, {0x01}, {{0x08}, {0x12, texture}});
            profileComponents = r::ReferenceField(profile, "components");
            r::Require(getBinding && setBinding && clipMode && cameraTarget && canvasMode && setVideoAudio && audioStop && setParent &&
                textureName && propertyId && getGlobalTexture && setGlobalTexture && profileComponents &&
                StageLoader::objectName && StageLoader::goTransform && StageLightingRuntime::setEnabled,
                "native composition binding or lifecycle interface unavailable");
            ready = true;
        }
        Rule Match(void* track) const
        {
            for (const auto& rule : rules)
                if (rule.track && r::Is(track, rule.track)) return rule;
            return {};
        }
    };
    inline Bindings bindings;
}
