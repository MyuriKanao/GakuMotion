#!/usr/bin/env python3
"""Read-only v31 surface check. Does not load/execute a game or validate visuals.

Runtime binding additionally checks parameter/result classes, value layouts,
generic/by-ref flags and ambiguity; this static check is not a substitute.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


class Metadata:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.header = struct.unpack_from('<64I', self.data)
        if self.header[:2] != (0xFAB11BAF, 31):
            raise ValueError('Expected IL2CPP metadata version 31')
        self.types = self.table(40, '<16i8H2I')
        self.methods = self.table(12, '<7i4H')
        self.fields = self.table(24, '<3I')
        self.nested = self.table(32, '<I')
        self.classes = {}
        for image in self.table(42, '<10I'):
            for index in range(image[2], image[2] + image[3]):
                definition = self.types[index]
                key = (self.string(image[0]), self.string(definition[1]), self.string(definition[0]))
                self.classes.setdefault(key, []).append(definition)

    def table(self, index, layout):
        start, length = self.header[index:index + 2]
        size = struct.calcsize(layout)
        if start + length > len(self.data) or length % size:
            raise ValueError('Invalid metadata table bounds')
        return list(struct.iter_unpack(layout, self.data[start:start + length]))

    def string(self, index):
        start = self.header[6] + index
        end = self.data.index(0, start, self.header[6] + self.header[7])
        return self.data[start:end].decode('utf-8')

    def find(self, assembly, namespace, name):
        matches = self.classes.get((assembly, namespace, name), [])
        if len(matches) != 1:
            raise ValueError(f'Class is absent or ambiguous: {assembly}:{namespace}.{name}')
        return matches[0]

    def methods_of(self, definition):
        return self.methods[definition[9]:definition[9] + definition[16]]


# Counts and static/instance ownership only. No code offsets, resource IDs,
# secret fields or parameter contents are extracted.
SURFACES = [
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'GameObject', '.ctor/1 AddComponent/1 GetComponentsInChildren/2'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Object', 'Instantiate/1s Destroy/1s op_Implicit/1s'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'ScriptableObject', 'CreateInstance/1s'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Transform', 'get_localPosition/0 set_localPosition/1 get_localRotation/0 set_localRotation/1 get_localScale/0 set_localScale/1'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Light', 'get_range/0 set_range/1 get_spotAngle/0 set_spotAngle/1 get_innerSpotAngle/0 set_innerSpotAngle/1 get_bounceIntensity/0 set_bounceIntensity/1 get_colorTemperature/0 set_colorTemperature/1 get_shadowStrength/0 set_shadowStrength/1 get_useColorTemperature/0 set_useColorTemperature/1'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Renderer', 'get_sharedMaterials/0 GetPropertyBlock/1 GetPropertyBlock/2 SetPropertyBlock/1 SetPropertyBlock/2'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'MaterialPropertyBlock', '.ctor/0'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine.SceneManagement', 'SceneManager', 'get_sceneCount/0s GetSceneAt/1s MoveGameObjectToScene/2s'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine.SceneManagement', 'Scene', 'GetNameInternal/1s GetIsLoadedInternal/1s'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine.Playables', 'PlayableAsset', ''),
    ('UnityEngine.CoreModule.dll', 'UnityEngine.Playables', 'DirectorUpdateMode', ''),
    ('UnityEngine.CoreModule.dll', 'UnityEngine.Playables', 'DirectorWrapMode', ''),
    ('UnityEngine.DirectorModule.dll', 'UnityEngine.Playables', 'PlayableDirector', 'get_playableAsset/0 set_playableAsset/1 set_playOnAwake/1 set_timeUpdateMode/1 set_extrapolationMode/1 Play/0 Stop/0 Evaluate/0 set_time/1 get_duration/0'),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'TimelineAsset', 'get_flattenedTracks/0 AddTrackInternal/1 set_fixedDuration/1 set_durationMode/1'),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'TrackAsset', 'get_clips/0 ClearSubTracksInternal/0 ClearMarkers/0 get_mutedInHierarchy/0 set_muted/1 get_parent/0 AddChild/1'),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'TimelineClip', 'get_asset/0 set_asset/1'),
    ('campus-submodule.Runtime.dll', 'Campus.Common', 'AssetBundleSceneLoader', 'LoadAssetBundleAsync/2 LoadSceneFromLoadedAssetBundleAsync/2 UnloadSceneAsync/1 UnloadAssetBundleAsync/1 CancelLoad/0 get_RootObjects/0'),
    ('campus-submodule.Runtime.dll', 'Campus.Live.Equipment', 'LiveEquipmentController', 'Initialize/0 BindReceiver/1 get_Receiver/0 Update/0 LateUpdate/0 Dispose/0 get_IsValidPlayableGraph/0'),
    ('campus-submodule.Runtime.dll', 'Campus.Live.Equipment', 'LiveEquipmentReceiver', 'ClearProperty/0 Update/0'),
    ('campus-submodule.Runtime.dll', 'Campus.Live.Equipment', 'EnvironmentBase', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Live.Equipment.Timeline', 'LiveEquipmentTrackBase', 'get_BpmInfo/0'),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusBpmInfo', 'EvaluateBpmAtTime/1'),
    ('campus-submodule.Runtime.dll', 'Campus.Live.Equipment.Timeline', 'LiveEquipmentClipBase', 'get_UseBpm/0'),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusBpmTimelineTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusBpmTimelineAsset', ''),
    ('UnityEngine.JSONSerializeModule.dll', 'UnityEngine', 'JsonUtility', 'ToJson/2s FromJsonInternal/3s'),
    ('Assembly-CSharp.dll', 'Campus.Common.Master', 'MasterManager', 'get_ProduceLiveMaster/0s'),
    ('Assembly-CSharp.dll', 'Campus.Common.Master', 'ProduceLiveMaster', 'GetAllWithSortByKey/1'),
    ('Assembly-CSharp.dll', 'Campus.Common.Proto.Client.Master', 'ProduceLive', 'get_MusicId/0 get_Type/0 get_EnvironmentAssetId/0 get_TimelineAssetId/0 GetMusic/0'),
    ('Assembly-CSharp.dll', 'Campus.Common.Proto.Client.Master', 'Music', 'GetDisplayTitle/0'),
    ('Octo.dll', 'Octo', 'OctoManager', 'get_Database/0s GetAllDependenciesOfAssetBundle/1s GetTotalAssetBundlesSizeToDownload/1s'),
    ('Octo.dll', 'Octo.Data', 'DataManager', 'GetAllAssetBundleNames/0 GetAssetBundleItemByName/2'),
    ('Octo.dll', 'Octo.Asset', 'AssetBundleOperator', 'GetAllScenePaths/0'),
    ('mscorlib.dll', 'System.Threading', 'CancellationTokenSource', '.ctor/0 get_Token/0 Cancel/0 Dispose/0'),
    ('mscorlib.dll', 'System.Threading', 'CancellationToken', 'get_None/0s'),
    ('UniTask.dll', 'Cysharp.Threading.Tasks', 'UniTask', 'get_Status/0 GetAwaiter/0'),
    ('UnityEngine.ParticleSystemModule.dll', 'UnityEngine', 'ParticleSystem', 'get_particleCount/0 get_time/0 set_time/1 get_randomSeed/0 set_randomSeed/1 get_useAutoRandomSeed/0 set_useAutoRandomSeed/1 get_isPlaying/0 get_isPaused/0 Play/1 Pause/1 Stop/2'),
    # Source-scene composition: environment, screens, audience and lifecycle.
    ('UnityEngine.DirectorModule.dll', 'UnityEngine.Playables', 'PlayableDirector', 'GetGenericBinding/1 SetGenericBinding/2'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Camera', 'get_targetTexture/0'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Shader', 'PropertyToID/1s'),
    ('UnityEngine.CoreModule.dll', 'UnityEngine', 'Behaviour', 'get_enabled/0 set_enabled/1'),
    ('UnityEngine.UIModule.dll', 'UnityEngine', 'Canvas', 'get_renderMode/0'),
    ('UnityEngine.VideoModule.dll', 'UnityEngine.Video', 'VideoPlayer', 'set_audioOutputMode/1'),
    ('UnityEngine.AudioModule.dll', 'UnityEngine', 'AudioSource', 'Stop/0'),
    ('UnityEngine.AudioModule.dll', 'UnityEngine', 'AudioListener', ''),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'AnimationTrack', 'get_inClipMode/0'),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'AnimationPlayableAsset', ''),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'ActivationTrack', ''),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'ActivationPlayableAsset', ''),
    ('Unity.Timeline.dll', 'UnityEngine.Timeline', 'GroupTrack', ''),
    ('Unity.RenderPipelines.Core.Runtime.dll', 'UnityEngine.Rendering', 'Volume', ''),
    ('Unity.RenderPipelines.Core.Runtime.dll', 'UnityEngine.Rendering', 'VolumeProfile', ''),
    ('Unity.RenderPipelines.Core.Runtime.dll', 'UnityEngine.Rendering', 'VolumeComponent', ''),
    ('Unity.RenderPipelines.Universal.Runtime.dll', 'UnityEngine.Rendering.Universal', 'DecalProjector', ''),
    ('Cinemachine.dll', 'Cinemachine', 'CinemachineVirtualCameraBase', ''),
    ('campus-submodule.Runtime.dll', 'CanvasRT', 'CanvasRenderTexture', 'GetTextureName/1s'),
    ('campus-submodule.Runtime.dll', 'Campus.Live.Equipment', 'EquipmentValue', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Common', 'CampusLightAnchor', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Common', 'CampusActorController', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Common', 'CampusVirtualActorController', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Common.Live', 'CampusLiveTimelineController', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusTimelineCriVideoPlayer', 'OnEnable/0'),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusLightTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusLightAsset', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusVolumeTimelineTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusVolumeTimelineAsset', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusVolumeProfileTimelineTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusVolumeProfileTimelineAsset', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusVideoPlayerTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusVideoPlayerClip', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusTargetImageTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Timeline', 'CampusTargetImageClip', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Crowd', 'CrowdVolume', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Crowd', 'CrowdTimelineTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.Crowd', 'CrowdTimelineAsset', ''),
    ('campus-submodule.Runtime.dll', 'Campus.AudiencePenlight', 'AudiencePenlightVolume', ''),
    ('campus-submodule.Runtime.dll', 'Campus.AudiencePenlight', 'AudiencePenlightTimelineTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.AudiencePenlight', 'AudiencePenlightTimelineAsset', ''),
    ('campus-submodule.Runtime.dll', 'Campus.AudiencePenlight', 'MobAudiencePenlightOverrideTrack', ''),
    ('campus-submodule.Runtime.dll', 'Campus.AudiencePenlight', 'MobAudiencePenlightOverrideAsset', ''),
    ('campus-submodule.Runtime.dll', 'Campus.MobAudience', 'MobAudiencePenlightController', ''),
]


def verify(path):
    metadata = Metadata(path)
    results = []
    for assembly, namespace, name, surface in SURFACES:
        owner = metadata.find(assembly, namespace, name)
        methods = metadata.methods_of(owner)
        for spec in surface.split():
            method_name, arity = spec.split('/')
            is_static = arity.endswith('s')
            count = int(arity.rstrip('s'))
            matches = [m for m in methods if metadata.string(m[0]) == method_name and m[10] == count
                       and bool(m[7] & 0x10) == is_static and m[5] < 0 and not (m[7] & 0x400)]
            if len(matches) != 1:
                raise ValueError(f'Method is absent or ambiguous: {namespace}.{name}.{spec}')
            results.append(f'{assembly}:{namespace}.{name}.{spec}')
    return {'metadata_version': 31, 'sha256': hashlib.sha256(metadata.data).hexdigest(),
            'classes_checked': len(SURFACES), 'methods_checked': len(results), 'surfaces': results,
            'runtime_invocation': 'not run', 'game_visual_acceptance': 'not run'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = verify(args.metadata)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Native Live metadata surface: {report['classes_checked']} classes, {report['methods_checked']} methods; runtime/visual not run")
