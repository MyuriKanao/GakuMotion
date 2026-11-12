# 扩展摄影舞台与程序化灯光

`0.1.2-dev` 新增的**原生歌曲灯控**见 [NATIVE_LIVE_LIGHTING.md](NATIVE_LIVE_LIGHTING.md)。本页保留 `0.1.1-dev` 程序化灯光的实现与验收记录；两种模式分别选择，互不冒充。

本地开发版：`0.1.1-dev`。未发布 Release，也未自动安装到游戏。

## 项目与实现范围

GakuMotion 通过 GakuLoader 执行 C++ 运行时；C# NativeAOT DLL 负责动作导入。用户提供的离线包主要回放本地协议数据，舞台与灯光仍由客户端 Unity 渲染。本次未修改离线包的服务器、资源缓存、启动器或运行中游戏文件。

摄影舞台继续使用原有 `PhotoBackground.GetEnvAssetId` 载入时替换，不另起场景加载事务、不接管歌曲的演出导演。新增列表与白名单共用 `GkStageCatalog.hpp`，从原有 3 项扩展至 16 项：

| 类别 | 入口 | 程序化灯光 |
| --- | --- | --- |
| Live | 通用 Live、初星 | 启用后驱动实际存在的灯具 |
| 中庭 | 日中、傍晚、日中变体 | 保留原生环境光 |
| 试镜 | 01～04 | 启用后驱动实际存在的灯具 |
| 练习室 | 舞蹈、形象、声乐 | 保留原生环境光 |
| 教室 | 清晨、日中、傍晚、夜间 | 保留原生环境光 |

这些入口是客户端元数据中存在的场景名称，不等同于当前安装已经下载对应资源，也不等同于逐项完成视觉验收。未将推测的歌曲专用场景名加入白名单。

## 使用

1. 在设置中开启开发者模式，进入 **[开发] → 摄影舞台替换**，选择下次载入的背景。
2. 重新进入偶像摄影，或使用游戏原生切换背景功能触发载入。修改选项本身不立即更换当前场景。
3. 载入通用 Live、初星或试镜舞台后，在 **舞台 → Live 舞台灯光（实验）** 展开设置。
4. 灯光默认选择 **原生（不接管）**。可改为固定染色、柔和呼吸、彩色渐变。配色有 8 套预设（霓虹、暖色、冷色、舞台白、粉彩、日落、全彩、粉白）和 **自定义**。自定义可设 2～4 个颜色，用取色器选择；拖动时只在面板内预览，松开后才保存并应用。设置写入 `stage.json` 的 `custom_count` / `custom_colors`，旧文件没有这两项时使用默认颜色。
5. **光影效果** 关闭或高级自定义节点开启时，自动灯光释放接管；F10 停止、退出页面、所有者变化或截图暂停也恢复原值。

灯光根据动作播放器的绝对时间采样：暂停不推进、跳转立即重采样、播放速度自然跟随播放器。BPM 是手工参数，不自动识别音频节拍。强度为 0～3 倍，BPM 为 30～240，灯光速度为 0.1～3 倍；呼吸每四拍一次，配色每八拍一轮，不使用硬切频闪。

**这不是 Agitato 或其他歌曲的原生 MV 灯光时间轴，也未新增缺失的 LED 动画、灯具或舞台资源。** 状态栏中的真实灯/体积光束数量是实际匹配到的组件数；出现 0 时应核对场景和资源，而不是据此认定灯光已完成。

## 运行时与恢复

- `GkStageLighting.hpp`：纯数据参数、确定性采样、原状态账本与恢复控制器。
- `GkStageLightingRuntime.hpp`：通过精确元数据解析 Unity `Light` 和 `VLB.VolumetricLightBeamSD`，分别驱动真实光照和可见体积光束。
- 灯光仅写入具有摄影载入凭据、相同摄影所有者和精确活动场景句柄的白名单演出舞台。
- 所有操作在 Unity 主线程执行。每次通过包含非活动对象的类型化枚举获取新引用，枚举数组的 GCHandle 在本次操作结束时释放。
- 跨帧只保存组件 ID、GameObject ID、场景句柄和颜色/强度/启用状态等原始值，不保存可再次解引用的组件指针。
- 保存原值后才执行 setter；setter 部分修改后失败仍进入恢复。枚举失败保留恢复账本，已销毁的精确身份在成功枚举后退役。
- 恢复包含真实灯颜色、强度、启用状态，以及体积光束内/外强度、颜色、颜色来源、强度来源和颜色模式。手动修改体积光属性后调用原生刷新方法。
- 没有角色回调时，宿主 tick 仍可重试退出后的恢复。不修改全局 RenderSettings、角色材质、共享舞台材质或歌曲时间轴。

舞台与灯光偏好写入既有 `Configs/gakumotion/stage.json`，沿用原子的临时文件替换与 `.bak`。旧版 version-1 文件没有 `live_lighting` 时仍以原生灯光读取；非法或非有限参数被拒绝。游戏资源引用及恢复账本不落盘。

## 来源与参数依据

- 场景名及组件/字段/方法名来自用户指定离线包的客户端静态元数据：`game/gakumas_Data/il2cpp_data/Metadata/global-metadata.dat`，格式 v31，SHA-256：`0951bf01ae553d59c426230b26d2f2b8baef10386ca3bd49f3d22b3123b29869`。
- `Light` 位于 `UnityEngine.CoreModule.dll`；体积光束与 `ColorMode` 位于 `quaunity-volumetriclightbeam.Runtime.dll`。运行时再次验证值类型尺寸、字段类型、非静态字段属性、组件继承关系及 `ColorMode.Flat == 0`；接口不匹配时保留原生路径。
- 静态元数据表的读取格式参考 [Il2CppDumper 的 MetadataClass 定义](https://github.com/Perfare/Il2CppDumper/blob/master/Il2CppDumper/Il2Cpp/MetadataClass.cs)。未引入该工具作为构建依赖，未复制游戏资产、用户动作、模型或录制数据。
- 调色板、强度范围、呼吸包络及周期均为本插件自定参数；青紫粉配色参考用户截图的方向，其余预设为手工选择，不宣称还原原生歌曲参数。

## 构建与合成验证（不等同于游戏验收）

使用根目录 `build.ps1` 构建；常规源码修改未准备源码发布快照，因此未重写 `freeze-manifest.json`。

```powershell
.\build.ps1 -Target Runtime
.\build.ps1 -Target Contracts
.\tests\run_release_tests.ps1 -WindowsSdkRoot 'C:\Program Files (x86)\Windows Kits\10'
```

2026-11-11 的验证记录：

| 检查 | 结果 |
| --- | --- |
| MSVC x64 Runtime | 已通过 |
| NativeAOT win-x64 配套导入器 | 已通过，使用固定 .NET SDK 8.0.427 |
| 核心合成回归 | 903 通过，0 失败 |
| 舞台载入 detour | 106 通过，0 失败，覆盖全部目录项 |
| 灯光控制器/设置/生命周期 | 85 通过，0 失败 |
| 生产灯光适配器的 IL2CPP 合成边界 | 14 通过，0 失败 |
| ImGui 语法 | 已通过 |
| GUI 真实控件交互与布局 | 161 通过，0 失败 |
| 数据布局 | 29 通过，0 失败 |
| Unicode 路径 | 15 检查通过，0 失败 |
| 真实 NativeAOT DLL 合成导入桥 | 43 检查通过 |
| 非零返回值/不兼容 ABI 拦截 | 6 检查通过 |

日志保存于 `build/validation/stage-lighting/`。GUI 测试使用会话内的临时 D: PowerShell 驱动名跳过脚本中不存在的 SDK 候选路径，未挂载系统磁盘或修改系统配置。

本机原先只有 8.0.403/8.0.412，首次 `All` 在固定 SDK 解析处停止；随后从 Microsoft 官方发布元数据下载 8.0.427 win-x64 SDK，校验 SHA-512 后解压至 `build/toolchains/dotnet-8.0.427`。未放宽 `global.json`、替换系统 SDK 或改变系统 PATH。完整配套构建命令：

```powershell
.\build.ps1 -Target All -OutputRoot 'F:\GakuMotion\build\local-0.1.1-dev' `
  -DotNet 'F:\GakuMotion\build\toolchains\dotnet-8.0.427\dotnet.exe'
```

输出为 `build/local-0.1.1-dev/native/GakuMotion.dll` 和 `build/local-0.1.1-dev/nativeaot/GakuMotion.VmdImporter.Native.dll`。插件描述和导入器版本均为 `0.1.1-dev`；导入算法及 ABI 未变更。完整构建日志为 `build/validation/stage-lighting/stage-local-all-build.log`；配套本地安装包和校验信息位于 `build/packages/`。

## 游戏载入与视觉验收（待执行）

本次只进行了本地构建与合成测试，没有替换运行中 DLL、启动/操作游戏或采集游戏画面。新增舞台的资源可用性、摄影位置、角色受光、体积光束和与其他插件的实际效果仍需游戏内验收。旧中庭日中仅继承此前用户反馈，不代表新增入口已测试。
