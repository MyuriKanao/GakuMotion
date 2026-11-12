# 原生 Live 演出组件装配（0.1.3-dev，本地开发版）

## 要解决的问题

2026-11-12 的只读资源检查（[Live 舞台调研报告](../build/validation/live-scene-audit/REPORT.md)）发现：抽查的 Live 001 / 012 / 019 中，灯具、灯控 receiver、大屏内容源、后期 Volume 和观众都在 `tln_live_*` 时间轴包里，`env_3d_live_*` 环境包本身没有 receiver。

旧实现会停用时间轴源场景的全部根节点，再只在配套舞台中寻找 receiver，并且只克隆设备轨和 BPM 轨。按这个资源布局，灯架、大屏和观众实体根本不会出现，环境渲染轨也不会被驱动。

## 当前行为

源时间轴场景仍由插件自有 loader 载入。载入后，`SourceComposition` 读取这个自有场景的完整层级，按根节点分类，只重新显示被允许的根：

| 层 | 显示的根（依据组件判定） | 驱动的轨道 | 开关 |
| --- | --- | --- | --- |
| 灯架 | 含 `LiveEquipmentReceiver`、Light、VLB、`EnvironmentBase`、`CampusLightAnchor`、`EquipmentValue` 的根 | 设备轨、BPM 轨（自有 director）；`CampusLightTrack`、灯架 AnimationTrack | 跟随原生灯光模式 |
| 环境渲染 | 含后期 `Volume`、网格渲染器、`DecalProjector` 的根 | `CampusVolumeTimelineTrack`、`CampusVolumeProfileTimelineTrack`、环境 AnimationTrack / ActivationTrack | 原生环境渲染 |
| 大屏 | 含 `CanvasRenderTexture` / `VideoPlayer` 的根，含其渲染贴图相机与 Canvas | `CampusVideoPlayerTrack`、`CampusTargetImageTrack`、屏幕 AnimationTrack | 舞台大屏 |
| 观众 | 含 `CrowdVolume`、`AudiencePenlightVolume`、`MobAudiencePenlightController` 的根，以及名为 Crowd / Mob / AudiencePenlight 的根 | `CrowdTimelineTrack`、`AudiencePenlightTimelineTrack`、`MobAudiencePenlightOverrideTrack` 及其 AnimationTrack | 观众与荧光棒 |

receiver 选择规则：配套舞台有唯一 receiver 时沿用旧路径，源灯架保持隐藏；配套舞台没有 receiver 时，使用源场景里唯一且已显示的 receiver。两边都歧义时报错，不猜。

### 始终不接管

以下内容即使和被允许的组件在同一个根下，也会让整个根保持隐藏，并在状态栏计数：

- 演员（`CampusActorController`、`CampusVirtualActorController`）、`CampusLiveTimelineController`；
- Cinemachine 虚拟相机、没有渲染贴图的 Camera、屏幕叠加层 Canvas；
- AudioSource / AudioListener；
- CRI 背景视频（`CampusTimelineCriVideoPlayer`）；
- 灯架之外的独立粒子根（其 ControlTrack 不克隆，显示出来只会不同步地发射）。

若 receiver 所在的根混入上述内容，会话直接失败，不会只显示一半灯架。

### 轨道宿主

非设备轨挂在它们原本的源 director 上运行，因此 `CampusVolumeTimelineAsset.volume` 这类 ExposedReference 仍能解析。每个 director 换成插件克隆的私有 TimelineAsset，只包含被允许的轨道；clip、playable asset 和 VolumeProfile（含其中的 VolumeComponent）都是私有副本，源资产不改。

`MasterTimeline/Light` 这类无其他组件的子 director 会被单独移到根级显示，它的父节点（Live 控制器、镜头、音频）继续隐藏。

无限 / 录制 AnimationClip、绑定到隐藏对象或场景外对象的轨道、未知轨道类型一律跳过。

### 时钟与顺序

所有源 director 都设为 Manual / Hold，从不 Play 原始 Live。每帧顺序：辅助 director 先以同一绝对时间 Evaluate，然后跑一轮原生设备更新。同帧重复采样会被跳过。暂停、跳转和曲库 BPM 重映射沿用原有设备轨的处理。

### 声音与媒体

- 屏幕 `VideoPlayer` 的音频输出设为 None，声音仍由外部播放器负责。
- 源 AudioSource 停止并禁用。
- `CampusTimelineCriVideoPlayer.OnEnable` 只在本次自有源场景内被拦截，避免触发隐式视频下载；其他场景照常转发。

### 释放顺序

1. 停止自有设备 director、隐藏驱动、释放设备控制器、清理 receiver；
2. 隐藏源场景所有根（含后移出的 director 和运行时新建的子对象）；
3. 停止辅助 director，换回原 TimelineAsset，销毁私有副本；
4. 恢复 `CanvasRenderTexture` 名下的 shader 全局纹理；
5. 恢复配套舞台快照；
6. 卸载自有源场景和资源包。

任一步失败都保留账本，下一帧重试，不启动新会话。

## 设置

`stage.json` 的 `live_lighting` 新增 `environment`、`monitor`、`audience` 三个布尔值，缺省为 true，旧文件可直接读取。类型错误会整份拒绝，不部分覆盖。修改任一开关会先完整释放旧会话再重建。

## 模块

- `GkNativeLiveComposition.hpp`：纯规则，不调 Unity。根节点分类、receiver 选择、轨道准入。
- `GkNativeLiveAuxiliaryBindings.hpp`：精确类、方法和枚举解析。
- `GkNativeLiveCompositionRuntime.hpp`：读取自有场景层级、标记、静音源组件、显示 / 隐藏根、shader 全局纹理恢复。
- `GkNativeLiveAuxiliaryClone.hpp` / `GkNativeLiveAuxiliaryRuntime.hpp`：私有 TimelineAsset 克隆、绑定复制、手动采样。
- `GkNativeLiveMediaGate.hpp` / `GkNativeLiveMediaPolicy.hpp`：CRI 视频 OnEnable 拦截。

## 来源与限制

- 根节点结构和组件分布来自本地缓存的 `tln_live_all-001/012/019` 只读静态检查；类型和方法签名来自 v31 静态元数据（SHA-256 `0951bf01…b29869`）。未复制游戏代码或导出资产。
- 只抽查了 3 首通用 Live。个别歌曲的根结构可能不同，此时对应根会被排除并在状态栏计数，不会错误显示演员或镜头。
- 试镜 03 / 04 场景没有配对的原生演出时间轴，这次改动不会让它们的大屏出现内容；那需要另外接入 `InGame3DManager.PlayMovieMonitor` 一类的试镜屏幕链路。
- 场景包内的光照贴图 / 光照探针 / RenderSettings 属于环境包本身，本功能不改写。
- CRI 背景视频（例如 Live 012 的 `BackGround/CRIMovie`）不接管。
- 合成测试在 Unity 边界使用替身对象，不能证明真实 PlayableGraph、Volume 混合、CanvasRenderTexture、人群 GPU 渲染的效果。

## 本地验证（2026-11-12）

| 检查 | 结果 |
| --- | --- |
| `build.ps1 -Target Runtime`（MSVC x64） | 通过，DLL SHA-256 `69D05EE5…B55149` |
| `build.ps1 -Target Contracts` | 10 组全部通过；新增装配规则 34 项、媒体拦截 34 项，图适配 72 项 |
| `tests/run_release_tests.ps1` | 核心 902 项及上述合同通过 |
| `tests/run_gui_ux_tests.ps1` | 181 项通过 |
| `tools/verify-native-live-metadata.py` | 81 类 / 126 方法 |
| 游戏载入、Live 画面、大屏内容、观众渲染 | **未执行** |

日志在 `build/validation/native-composition/`，DLL 在 `build/native-composition-dev/native/`。没有部署到游戏。
