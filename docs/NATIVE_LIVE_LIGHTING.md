# 原生歌曲灯控（0.1.2-dev，本地开发版）

> 本文保留 0.1.2-dev 的历史实现与验证记录。0.1.3-dev 的当前选歌方式、BPM 重映射及目录修复见 [曲库节拍驱动灯光](LIBRARY_TEMPO_LIGHTING.md)；不再需要在舞台页另选播放歌曲。演出包内灯架、环境渲染、大屏和观众的装配见 [原生 Live 演出组件装配](NATIVE_LIVE_COMPOSITION.md)；下文“禁用源根节点、只克隆设备/BPM 轨”的描述已被其取代。

## 实现范围

此版本新增“原生歌曲时间轴”，复用客户端的 `LiveEquipmentController`、`LiveEquipmentReceiver`、设备轨道及 BPM 轨道，不再以手工配色/BPM 模拟原生曲目。外部动作仍由原播放器驱动，现有相机控制保持独立；原生演员、相机、音频、Control、Signal 等轨道不加入自有图。

这表示**原生灯控接入代码已实现**，不表示已经完成 Agitato 或所有舞台的逐帧视觉还原验收。没有启动、操作游戏或安装本版本。

## 使用顺序

1. 打开 **舞台 → Live 舞台灯光（实验）**，将灯光效果设为 **原生歌曲时间轴**。
2. 点击 **刷新本地曲目**，选择 Agitato 及其对应演出版本（如 TrueEnd）。曲名与版本由客户端 Master 表读取，不按曲名猜测资源 ID。
3. 重新进入偶像摄影。该模式优先替换成这首歌的配套环境，而不是沿用固定场景下拉框选择；退出此模式后仍保留原固定舞台偏好。
4. 播放外部动作。灯控使用播放器绝对时间；暂停保持时间，跳转直接重采样。可调时间轴偏移，程序化配色、BPM、强度参数在此模式禁用。
5. 缓存缺失、舞台不匹配、设备 receiver 不唯一或接口未匹配时显示原因并保持待机；排除原因后点击 **重试原生灯光**。不会自动下载或用 Agitato/程序化灯光冒充其他歌曲。

其他歌曲同样从 Master 的 **MusicId + ProduceLiveType → EnvironmentAssetId + TimelineAssetId** 配对，因此允许不同歌曲使用不同舞台。配对存在歧义的记录不进入曲目列表。

## 模块与生命周期

- `GkNativeLive.hpp`：线程安全值类型目录、资源名校验、绝对时间映射及加载/恢复状态机。
- `GkNativeLiveReflection.hpp` / `GkNativeLiveBindings.hpp`：精确 IL2CPP 方法/字段/布局解析。只对闭合 `List<ProduceLive>` 的非泛型访问器允许 inflated 方法；没有放宽项目通用解析器。
- `GkNativeLiveCatalogRuntime.hpp`：读取 Master 和 Octo 目录；资源名称必须实际存在于目录，依赖包下载量必须为 0 才开始载入。
- `GkNativeLiveSceneRuntime.hpp` / `GkNativeLiveTaskRuntime.hpp`：使用自有 `AssetBundleSceneLoader` 加载时间轴场景，禁用源根节点。成功、故障、取消的 UniTask 都消费一次。已有同名场景不采用、不卸载；只释放由自有 loader 及新场景身份共同证明的场景/包。
- `GkNativeLiveClonerRuntime.hpp`：新建 TimelineAsset；只克隆原生设备和 BPM 轨、独立 TimelineClip 及 playable asset；保留设备子轨层级、静音、片段时间与原生 BPM 逻辑。剥离 marker 通知，不改写原始轨道或 clip。无法证明克隆隔离则终止。
- `GkNativeLiveGraphRuntime.hpp` / `GkNativeLiveHooksRuntime.hpp`：自有 Manual/Hold PlayableDirector 与原生设备控制器；只抑制自有控制器及所绑定 receiver 的自动帧回调，其他游戏组件始终转发。每个外部动作帧执行完整的原生设备更新周期。
- `GkNativeLiveSnapshotRuntime.hpp` 及 Objects/Materials/Particles 模块：保存并恢复目标舞台的 Transform/活动状态、环境脚本/VLB 序列化值、Light 属性、材质属性块、粒子数组/时间/随机种子/播放状态。内置 Light 不使用 JsonUtility。停止后先完成原生控制器释放和 receiver 清理，再恢复快照。

Unity 操作只在主线程进行。自有对象和异步结果使用强 GCHandle；每次操作重新获取其目标，目标舞台恢复采用新枚举的组件/GO/场景身份。保存的是本次会话的恢复信息，不落盘为用户素材。所有者变更、停止、截图暂停、切曲或设置变更均先释放旧会话；恢复或卸载尚未完成时保留账本，阻止启动新会话。故障会锁存，避免逐帧重新加载。

偏好继续使用 version-1 `stage.json`，新增 `live_lighting.song_key` 和 `native_offset`，旧文件保持兼容。动态配套舞台不写进原有固定场景字段，防止重启时曲目目录尚未就绪导致整份设置被拒绝。

## 来源、校验与限制

- 游戏接口名/类型关系来自用户指定安装的只读静态 v31 元数据，SHA-256：`0951bf01ae553d59c426230b26d2f2b8baef10386ca3bd49f3d22b3123b29869`。未复制游戏算法实现、舞台资产、曲目数据或用户动作/模型/截图。
- 曲目与环境/时间轴资源 ID 在运行时从 Master 与 Octo 读取；没有硬编码推测的 Agitato ID。程序化灯光参数来源与本功能分离。
- Unity 序列化调用语义参考 [Unity JsonUtility 源码](https://github.com/Unity-Technologies/UnityCsReference/blob/master/Modules/JSONSerialize/Public/JsonUtility.bindings.cs)。当前客户端裁剪了公开 FromJsonOverwrite 包装器，故绑定其保留的、带目标对象及实际 Type 的精确 `FromJsonInternal` 入口；只用于本次捕获的 MonoBehaviour。
- Timeline 父子/clip 语义参考 Unity Timeline 包源码。原生 clip 的 parent setter 会操作旧轨道，故仅在独立克隆上设置已验证的 parent 引用字段，避免接触源轨道。依赖许可证和现有归属声明保持不变。
- 静态 surface 检查只验证类、方法名、参数数量、静态性及唯一性；运行时另行校验完整签名、布局与对象身份。二者都不替代游戏内验收。
- 源时间轴场景使用游戏原生异步加载器，载入完成即禁用源根节点。源 prefab 的 Awake/OnEnable 行为、设备 ID 覆盖、native job 与其他插件的时序、全曲灯效及跳转后的粒子表现仍须真实客户端验收。
- 主线程停止/恢复支持异步清理重试。若宿主退出时不再投递主线程 tick 且资源仍在释放中，会记录 pending 并保留身份/根，不以工作线程调用 Unity 或卸载别人的场景掩盖失败。

## 本地验证

使用 `build.ps1 -Target All` 构建 Runtime 和 NativeAOT；`build.ps1 -Target Contracts`、`tests/run_release_tests.ps1`、`tests/run_gui_ux_tests.ps1` 验证合成输入及真实 ImGui 控件交互。`tools/verify-native-live-metadata.py` 只读校验静态元数据，不运行游戏。

构建与验证结果单独记录在 `build/validation/native-live/`，产品 DLL 位于 `build/local-0.1.2-dev/`。本版本未自动覆盖游戏插件，Agitato/其他歌曲的游戏内加载与视觉验收均为待执行。

| 2026-11-11 本地检查 | 结果 |
| --- | --- |
| MSVC x64 Runtime / .NET 8.0.427 NativeAOT | 通过 |
| 核心合成回归 | 903 通过 |
| 摄影载入 detour | 106 通过 |
| 程序化灯光 / 原生基线账本 | 89 通过 |
| Light/VLB 生产适配器合成边界 | 14 通过 |
| 原生目录、会话、设置、时钟 | 43 通过 |
| 生产图克隆、自有帧钩子及 driver（合成 Unity 边界） | 32 通过 |
| 生产 UniTask 消费器（合成 source） | 11 通过 |
| 真实 ImGui 控件交互 | 168 通过 |
| 数据布局 / Unicode 路径 | 29 / 15 通过 |
| 真实 NativeAOT 合成导入桥 / 错误码门禁 | 43 / 6 通过 |
| 只读静态 metadata surface | 36 类、111 方法 |
| 游戏载入 / Agitato 画面 / 其他歌曲 | 未执行 |

图适配测试在 Unity 边界使用合成对象，完整快照在图测试中为替身；JSON、材质属性块、粒子原生恢复、真实 native jobs 及异步场景载入的运行效果仍须游戏内验证。

## 曲目目录读取修复

用户界面报告 `native string exceeds limit`，游戏日志确认原生接口元数据已匹配，但目录刷新未完成。旧实现对整个 Octo 目录应用单条 256 字符硬限制，并让单个异常字段中止整张曲目表；截图未揭示具体是哪一条原生记录触发限制。

本地修复保留有界读取：先验证 System.String 类型；全局资源名接受最多 4096 字符/字节，超界、空值、错误类型和内嵌 NUL 的名称跳过，不截断名称或制造资源 ID。曲目关键字段异常只跳过本条；超长标题回退为有效 MusicId。必需的依赖包仍严格验证长度、目录成员关系及本地缓存，未放宽实际加载条件。失败刷新保留上次有效目录，日志只记阶段与计数，不输出原生记录内容。

列表为空时下拉框禁用并提示刷新；“刷新本地曲目”保持可用。“重试原生灯光”针对灯控会话，不替代曲目目录刷新。

新增生产目录读取器/字符串边界的 18 项合成回归，GUI 控件交互合计 171 项通过，核心及既有灯控测试保持通过。修复产物为 `build/local-0.1.2-catalog-fix/native/GakuMotion.dll`，描述版本仍为 `0.1.2-dev`，通过源码提交和 SHA-256 区分构建；NativeAOT 导入器及 ABI 不变。此记录不代表新构建已部署、真实目录已刷新或灯效已验收。
