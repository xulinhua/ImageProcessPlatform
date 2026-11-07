# VisionPlatform 产品需求文档（PRD）

| 字段 | 内容 |
|---|---|
| 产品 | VisionPlatform（通用工业视觉平台） |
| 仓库 / 方案 | `C:\Git\other\ImageProcessPlatform\VisionPlatform\VisionPlatform.sln` |
| 启动工程 | VisionApp → `VisionPlatform.exe` |
| 技术栈 | VS2022（v143）+ C++17 + Qt 5.15.2（Windows） |
| 文档依据 | `VisionPlatform/README.md`、`docs/architecture.md`、`docs/3d-tools.md`、`docs/15-工具插件方案.md`、`docs/versioning.md`；上级 `ImageProcessPlatform/docs/` 下项目评估、阶段对比、许可 / 远程 / 运行方式 / 三维 / AI / 配置加密 / 混淆方案 |
| 版本基准 | 软件版本字段见 `VisionVersion.h` / `platformVersion`（README 口径约 `1.0.0`） |
| PRD 版本 | 1.0 |
| 日期 | 2026-11-05（用户时区 UTC+8） |
| 范围决策 | ① 导图全量（底层基础 + 辅助功能 + 生产数据 + 优化项，含远程控台、许可、3D/AI、MES）；② 验收分期：单机 → 多机 Station/Link → 远程控台；③ 真相机 SDK 至少接入一家；运控 / IO 本阶段只保留设备台账，不接完整 SDK |

### TL;DR

VisionPlatform 要把「能编、能打开方案、能用文件源跑通设计/运行/复检」的骨架，做成产线可交付的通用视觉平台：方案→工站→流程→工具插件链稳定出图与判定，真相机可上机，批次结果可追溯，本机可拆成 Station/Link 多机，并具备远程控台、软许可、三维与 ONNX 推理、MES 上报等能力。目标用户是视觉工程师（配方案）、产线操作员（跑线）、现场管理员（账户/许可/远程）以及厂家实施与签发人员。本 PRD 只定义产品与验收，不改动任何业务源码。

---

## Goals

### Business Goals

1. **可上机交付**：单机场景下，至少一家真相机 SDK + 文件源双路径，配合现有 2D 工具链，能稳定完成「采集→定位/检测→判定→通信输出」闭环。
2. **可扩展售卖**：工具/算法一工具一 DLL（`VisionPluginAPI` apiVersion=1），Halcon / OpenCV / PCL / Open3D / ONNX 可按客户授权与依赖裁剪装包。
3. **可多机扩产**：同一套 `*.vpsol` + `deploy.json`，经 Vision.Station / Vision.Link 把多工站拆到多 PC，中控看汇总画面与 OK/NG。
4. **可远程运维**：RemoteMaster / RemoteClient / RemoteRelay（VRMT v2）支持监控墙、文件、审计与生产保护，缩短现场出差。
5. **可授权变现**：软许可（申请码 / 注册码 / `license.dat`）跑通启动与运行中失效倒计时；硬件狗预留同一 `LicenseSource` 基类。

### User Goals

1. **视觉工程师**：在设计态用画布连线、试跑本工具/本流程、ROI 调参，保存方案后切运行态连续出图，不必改代码。
2. **产线操作员**：登录后只见运行/复检必要操作；看产量、良率、当前图与 NG 提示；故障信息可读、可复检。
3. **现场管理员**：管账户权限、许可注册、本机节点角色（standalone/server/client）、远程准入与审计。
4. **厂家实施 / 签发**：用 LicenseSign 离线签发；用安装脚本与文档把 Station/Link/Remote 装到现场网。

### Non-Goals

1. **本阶段不做完整运控 / IO SDK**：设备台账可登记控制卡 / IO / 光源（并入控制卡类），不做轴运动、IO 读写运行时；标定与运控轴联动不在本阶段验收。
2. **不做 AI 训练与标注工具链**：只做产线 ONNX 推理（分类 / 检测 / 分割）；不做 TensorRT/CUDA 专版、实例分割、异常热力图（见 `18-AI方案.md` §12）。
3. **不做浏览器 / 手机远程客户端、不做 RDP 虚拟会话、不做远程命令行**（见远程方案 §17）；远程只看物理控制台桌面。
4. **本 PRD 不修改任何工程源码**；实现另立迭代。也不把「阶段对比文档里已过时的未做项」当作当前代码事实（见下文「文档口径」）。

### 文档口径（阶段对比 vs 现状）

`docs/视觉平台开发阶段对比.md`（2026-10-27）对照导图叶子，**以当时源码扫一眼为准**，若干「未做」已被后续 README / 工程推翻，写 PRD 时必须按 **README + `.sln` + 专题方案** 校正：

| 导图条目（阶段对比结论） | 当前仓库事实（以 README / 工程为准） |
|---|---|
| 加密狗 / 许可「未做」 | 已有 `VisionLicense`、`Vision.LicenseRequest` / `Vision.LicenseSign`，软许可流程写在 README §8；硬件狗仍为空子类 |
| 参数配置加密「未做」 | 已有 `VisionCfgCrypt` 工程与方案文；落地深度以评估/方案为准，不能再写「完全没有模块」 |
| 代码混淆「未做」 | 已有 `VisionObfus` + `Vision.ObfusTool`；多数业务尚未挂接 VM，加壳仍不做 |
| 客户端+服务端 / 无窗口「未做」 | 已有 `Vision.Station` / `Vision.Link`、`node.json`、`--console`；托盘与服务化按运行方式方案验收 |
| 新算法 3D / AI「未做」 | `.sln` 已含大量 `Plugin.Tool.*3D*` / `Plugin.Alg.Pcl|Open3d.*` 与 `Plugin.Alg.Onnx.Infer.*`；`3d-tools.md` 标明多数可编译，相机/手眼等仍有占位失败 |
| 真相机「部分 / SDK 未接」 | **仍成立**：`VisionGrabFile` 文件源可用；CamSDK 筛选器以文件采集为主，真相机需本 PRD P0 接入至少一家 |

评估文（`VisionPlatform项目评估.md`）里的调度缺陷（S1 失败策略、S2 触发、S3 连线真源、U1 停循环卡 UI、A1 口令等）**仍是交付风险**，纳入本 PRD 的稳定性要求，不因「有远程/有 3D 工程」而自动关闭。

---

## User Stories

### 视觉工程师

- 作为视觉工程师，我想用文件源或真相机在设计态试跑采集→定位→判定全流程，以便在没量产节拍前把方案调通。
- 作为视觉工程师，我想把工具拖到画布并用端口连线（图/区域/位姿/逻辑），以便复用同一套 Job 结构做多工站。
- 作为视觉工程师，我想在缺 Halcon 或某算法 DLL 时看到明确「未登记」错误而不是静默空结果，以便区分环境问题与参数问题。
- 作为视觉工程师，我想加载点云 / 跑拟合平面 / 看三维窗，以便在三维工位上验证几何结果。
- 作为视觉工程师，我想挂 ONNX 分类/检测/分割模型（模型文件在 `toolDataDir`，不进 vpsol），以便把客户已有模型接到产线。

### 产线操作员

- 作为操作员，我想一键进入运行态看当前画面、产量、合格、不合格与良率，以便盯线。
- 作为操作员，我想在 NG 时立刻看到提示，并在复检里按文件夹重跑或按历史记录还原（生产数据阶段），以便确认是真缺陷还是参数问题。
- 作为操作员，我不想碰到设计态改参入口（权限控制），以免误改方案。

### 现场管理员

- 作为管理员，我想管理 admin/engineer/operator 账户与权限位，并在 Release 包强制登录，以便满足现场权限审计。
- 作为管理员，我想完成本机软许可申请与注册，并在许可失效时看到倒计时而非突然黑屏，以便有窗口联系厂家。
- 作为管理员，我想把本机设为 standalone 或 server/client，拉起 Station/Link，并维护 `deploy.json`，以便多机扩产。
- 作为管理员，我想用远程中控监控多台被控端桌面、传文件、看审计，并在生产中触发现场确认，以便远程协助不停线失控。

### 厂家实施 / 签发

- 作为厂家签发人员，我想用私钥工具根据申请码签发注册码（永久或到期日），以便离线授权客户机。
- 作为实施工程师，我想按安装脚本配置防火墙与 RemoteRelay，以便跨网段仍能配对运维。

---

## Functional Requirements

按产品域分组，优先级：**P0** 单机可交付硬门槛；**P1** 多机 Station/Link；**P2** 远程与许可加固；**P3** 生产数据 / MES / 统计；**P4** 3D/AI 产线可用与优化项收口。导图四块全覆盖，但验收仍按单机→多机→远程分期闸门。

### A. 方案、工站、流程、工具（底层基础 · Priority: P0）

- **领域模型：** 保持 `Solution → Station → Job → Tool`；方案文件 `*.vpsol`；设备台账、通信通道、全局变量、UI 页、`platformVersion`（见 architecture.md）。
- **设计态：** 资源树、Job 画布连线、执行本工具/本流程、`ToolDebugHost` 调参。
- **运行态：** 每工站 `StationLoop` 循环出图与计数；停止不得在 UI 线程无限 `join`（评估 U1）。
- **复检：** 离线文件夹批量、结果表、CSV/JSON；后续阶段升级为「历史记录还原」（见生产数据）。
- **失败策略：** `FailStopStation` / `FailSkip` / `FailNgContinue` 保存与运行一致；未实现的选项不可保存（评估 S1）。
- **触发：** 按流程解析循环/等待/文件夹采集，禁止混合站一律收成连续跑（评估 S2）。
- **连线真源：** 工具输入边为持久真源，执行树不得回写连线（评估 S3）。
- **版本：** 打开方案时校验 `platformVersion` 与工具 `toolVersion`；不认识的格式拒绝或明确降级提示（阶段对比建议项 + versioning.md）。

### B. 设备与采集（底层基础 · Priority: P0 相机；运控 P4 台账）

- **文件源：** 保留 `VisionGrabFile` / samples 路径，作为离线与 CI 基线。
- **真相机 SDK（必须）：** 至少接入 **一家** 工业相机 SDK（工程筛选器已出现 Hik / Hray 命名空间意向；实现落在 CamSDK / 采集工具链）。能力含：枚举设备、打开/关闭、取流、软触发/硬触发（按 SDK 能力）、曝光增益等常用参数、异常与断线提示。
- **产品层提示：** 未接 SDK 的后端不得静默空图；须返回可读错误（对齐既往「相机 SDK 未接入」行为，接入后改为成功路径）。
- **光源：** 无独立驱动亦可；若台账保留，须标明「仅台账 / 未驱动」。
- **运控卡 / IO 卡（本阶段）：** **只预留设备台账与工站芯片展示**；无轴运动、无 IO 读写 SDK。PRD 验收不要求运控联动标定走点。

### C. 通信与 PLC（底层基础 · Priority: P0/P1）

- **已有通道：** `VisionPort` 串口 / TCP / UDP / Modbus 主从接口继续可用。
- **流程通信工具：** 在现有 `writeRegs` / `waitRegs` / `writePose` 上补齐现场常用功能码清单与断线重连策略文档化行为。
- **PLC：** 继续「登记 PLC + 绑通道」；不强制品牌私有协议栈；以 Modbus 为第一验收协议。

### D. 辅助功能：日志、权限、数据库、标定、版本（Priority: P0/P1）

- **调试日志 / 操作审计：** 保留 `LogKindDebug`、`LogKindOp` + SQLite 审计。
- **生产日志：** 按节拍落盘（与结果库联动，见生产数据）。
- **用户权限：** 三角色保留；Release 强制登录；口令升级至方案要求的 PBKDF2（评估 A1）；关闭 Debug 绕过（U4）。
- **数据库：** `VisionDb` 上建立生产结果库（非仅 `vision.db` 日志）。
- **标定：** 现有标定板 / 九点 / 旋转中心 / 位姿映射 / 畸变可继续用图像与点表；**不要求运控轴走点**（本阶段 Non-Goal）。
- **配置加密：** 按 `配置加密方案.md`，`*.vpsol` / settings / users / composites 读写经 `VisionCfgCrypt`；旧明文可读、再保存为密文。
- **方案升级降级：** 明确拒绝未知主版本；工具参数与 DLL 版本不一致时标明。

### E. 生产数据与 MES（生产数据 · Priority: P1–P3）

- **当次计数：** 运行页产量 / OK / NG / 良率 / 节拍（已有内存累计）→ 须落库并带批次 / 班次 / 工单字段。
- **统计与图表：** 历史曲线、分布；CPK / GRR 作为 P3 可开关模块。
- **单料记录：** 图像 + 定位/检测测值 + 条码/时间可查。
- **数据还原：** 按记录恢复当时图与叠加（区别于「仅重跑文件夹」）。
- **复检导出：** 在还原之上支持客户配置的 CSV / Excel / HTML / PDF（阶段对比缺口）。
- **离线参数对照：** 同批数据改参前后对比（P3）。
- **MES 上传 / 查看：** 提供可配置的结果上报接口（字段映射、触发时机、失败重试与本地缓存）；MES 查看以只读查询或外链为主。协议细节可适配客户，PRD 要求「有抽象上传口 + 至少一种可演示适配（如 HTTP JSON 或文件投递）」。

### F. 优化项：许可、安全、运行形态、高速、UI（Priority: P0–P2）

- **软许可（P0/P2）：** 对齐 `许可方案.md` 与 README §8：启动 `open` 失败不进主窗；运行中 `poll` 失败倒计时（默认 10 分钟可配）；到点停工站；申请码 `VP-A1-` / 注册码 `VP-R1-`；许可证 `%ProgramData%\VisionPlatform\license\license.dat`；三处时间戳防回拨。
- **硬件狗：** `DongleLicense` 空子类保留；本阶段不接狗 SDK。
- **代码混淆：** 库与工具可随包；业务函数挂接与加壳不作为本阶段强制验收（方案已定「先有模块」）。
- **良率报警：** NG 数 / 良率阈值 → 提示或停机（可配）。
- **普通 / 高速模式：** 高速关闭冗余调试日志与非必要 UI 刷新；工具/算法可查询当前模式。
- **运行界面形态：** 单进程主窗（已有）；控制台 `--console`；托盘驻留；Station 可服务化。无窗口 / 托盘行为以运行方式方案 §14 为准。
- **可配置 UI：** 保留 ParamField/GenericEditor；现场自由拖控件运行画面列为后续，不挡 P0。

### G. 本机多端 Station / Link（优化项·分布式 · Priority: P1）

- **三程序：** `VisionPlatform.exe`（界面总控）、`Vision.Station.exe`（跑本机工站，可存原图）、`Vision.Link.exe`（CommFrame 收发，不跑工站、不存原图）。
- **角色：** `node.json` 中 `standalone` / `server` / `client`；非 standalone 使用 `VisionIpc` 共享内存。
- **部署：** 方案旁 `deploy.json` 映射 stationId → nodeId。
- **验收锚点（运行方式方案 §14）：** 托盘驻留、注销不影响 Station 存图、三机三工站汇总 JPEG+叠加+OK/NG、Link 断开约 3 秒标离线、停止本机工站不影响 Link、重复 stationId 拒绝等。
- **第一版不做：** Link 内另写套接字栈、原图跨机、断线堆帧、节点 TCP 登录加密等（方案 §13）——PRD 继承为 Non-Goals。

### H. 远程控台 VRMT（Priority: P2）

- **程序：** `RemoteMaster.exe` / `RemoteClient.exe` / `RemoteRelay.exe`；协议库 `Vision.Remote`，magic `VRMT` version 2。
- **能力基线：** 远程方案 §16 标明一至六期多项「已完成」——PRD 验收按 §18 A1–A10 抽样固化：监控墙多路、断网恢复、单画面操作、本地键鼠优先、防爆破、锁屏/UAC、回连、大文件续传、TLS 抓包不可明文、长稳内存。
- **与平台耦合：** `IRemoteAuth`、生产保护、格子显示工站状态（第六期口径）。
- **不做：** 声音、移动端、云中继依赖、远程 shell（§17）。

### I. 三维（Priority: P1–P4）

- **工具大类：** Acquire3D / Prepare3D / Locate3D / Measure3D / Detect3D / Calib3D（见 `3d-tools.md`）。
- **算法：** 默认 PCL，可切 Open3D；无 DLL 明确失败，无软兜底。
- **显示：** 点云窗旋转缩放、几何叠加（三维方案验收）。
- **硬件：** `acquire3d.camera` / 手眼等占位须在文档标明；真三维相机接入可与 2D 真相机分期，但 P0 不阻塞「点云文件→拟合平面」闭环。
- **端口：** PortCloud / PortPose3 / PortShape3 等按三维方案落地，旧二维方案行为不变。

### J. AI 推理（Priority: P1–P4）

- **工具：** `infer.classify` / `infer.detect` / `infer.segment` + 对应 `Plugin.Alg.Onnx.*`。
- **运行时：** 进程内 ONNX；会话在公共动态库按模型路径缓存（AI 方案）。
- **验收：** AI 方案 §11 八条（工具箱可见性、维度校验、NMS、分割区域、多 toolDataDir 不串会话、模型不进 vpsol 等）。
- **不做：** 训练、TensorRT、实例分割、旋转框等（§12）。

### K. 插件与算法生态（贯穿）

- Tool DLL 与 Algo DLL 同 `toolName`；宿主只依赖 PluginAPI 与基类。
- 算法族：Halcon / OpenCV / PCL / Open3D / ONNX；覆盖不齐时打开方案标出缺 DLL（评估 P2）。
- 构建：x64 Debug/Release；`CopyQt.bat` 拷运行库；无第三方则对应 Alg 不加载。

---

## User Experience

**Entry Point & First-Time User Experience**

- 安装或编译出 `bin\<Platform>\<Configuration>\VisionPlatform.exe`，旁路 `plugins/`、`samples/`、`icons/`、`data/`。
- Release：登录框（空库内置 admin/engineer/operator，默认空口令须强制改密——产品要求）；许可无效则申请/注册向导，不进主窗。
- Debug：可开发绕过，但交付清单禁止现场 Debug 包。
- 首次进入：示例方案或空方案；设备默认可指向 `samples/` 文件源；帮助→版本管理 / 关于可查模块与工具版本。

**Core Experience**

- **Step 1 设计方案：** 建工站与 Job → 从工具箱拖采集/预处理/定位/检测/判定/通信 → 画布连线 → 选文件源或真相机 → 本工具/本流程试跑 → 看图与结果表 → 保存 `*.vpsol`。
  - 校验：端口类型不匹配不可连；缺算法 DLL 明确失败文案。
- **Step 2 切运行态：** 启动工站循环 → 主画面刷新 → 计数与良率 → 通信输出。
  - 停止：UI 保持可响应；许可失效出现置顶倒计时。
- **Step 3 复检：** 选目录或历史批次 → 浏览 NG → 导出或跳回设计调参。
- **Step 4 多机（P1）：** 配 `node.json` + `deploy.json` → 托盘拉起 Station/Link → 服务端运行墙看多路。
- **Step 5 远程（P2）：** 中控发现/按 ID 连接被控 → 监控墙 → 申请操作权 → 文件与审计。

**Advanced Features & Edge Cases**

- 真相机断线、Modbus 从站超时、方案版本过新、许可回拨、重复 stationId、远程生产保护拒绝操作、ONNX 模型维度不符、点云二进制 PLY 拒绝等，均需可读错误与审计/日志落点。
- 高速模式：画面降刷新，日志降噪，不影响判定口结果。

**UI/UX Highlights**

- 三工作区清晰隔离；操作员角色隐藏设计入口。
- HsImage 2D ROI 与三维点云页切换不破坏旧方案。
- 远程与主程序视觉风格可独立，但状态色（在线/离线/NG）语义一致。
- 中文为默认界面语言（`VisionLang` / `lang/`）。

---

## Narrative

某消费电子结构件工厂要上多工位外观检。工程师在单机上用 VisionPlatform 打开方案，先用 `samples` 把找圆、Blob 和判定规则调通，再换成产线相机（已接的一家 SDK）验证曝光与触发。方案存成 `*.vpsol` 后切到运行态，操作员只看到画面和良率；NG 被写入批次库，第二天可以用复检把当时的图和叠加调出来给工艺看。产线扩到三台工位机时，实施把两台设为 client、一台 server，Station 继续在本地跑节拍，Link 只送压缩画面和结果，办公室的服务端墙上三路状态一目了然。某夜良率骤降，管理员用 RemoteMaster 连上工位桌面，在生产保护确认后协助改参，全程审计可查。厂家通过软许可控制部署套数，三维与 ONNX 工具在有依赖的工位按需加载。平台不再是「只能演示文件源的骨架」，而是可以卖、可以装、可以远程养的视觉操作系统。

---

## Success Metrics

### User-Centric Metrics

- 新项目工程师在 **1 个工作日** 内用文档 + 示例完成「文件源→定位→判定」方案保存并运行。
- 操作员培训后 **30 分钟内** 独立完成开线、停线、查看 NG。
- 远程协助首次连接成功率 ≥ 95%（局域网）。

### Business Metrics

- 单机标准检方案可签产线验收（真相机路径）。
- 同一方案模板可复制到 ≥ 3 台工位机（Station/Link）而不改工具 DLL。
- 许可签发到客户注册成功闭环 ≤ 1 个工作日（离线）。

### Technical Metrics

- 单工站连续跑 **72 小时** 无内存单调上涨、无界面卡死（停循环可响应）。
- 真相机取流失败率在稳定光照下按现场定义 ≤ 约定阈值；失败可诊断。
- 远程监控墙 16 路场景满足远程方案 A1 CPU 约束；断网恢复 ≤ 3 秒级（方案口径）。
- `VisionCoreTest` 及后续调度回归（S1/S2/S3）在 CI 或本地一键非 0 失败可发现。

### Tracking Plan

- 启动成功 / 许可失败原因码
- 工站 start/stop、每拍 OK/NG、节拍耗时
- 相机 open/grab 错误码
- 方案打开拒绝（版本）次数
- Station/Link 心跳与离线
- 远程会话建立、操作权申请、审计事件
- MES 上报成功/失败/重试
- 工具执行失败（未登记算法 / 模型缺失）分类计数

---

## Technical Considerations

### Technical Needs

- 保持分层：VisionApp/Ui → VisionCore → ToolBase/AlgBase → plugins；远程与许可为旁路可执行文件 + DLL。
- 采集抽象：文件源与真相机共用设备台账 `deviceId`；SDK 细节封在 CamSDK / 采集插件。
- 结果库：SQLite（或可替换）与 `data/` 目录约定；原图策略（存路径 / 缩略图 / 满图）需可配以免磁盘爆。
- 插件扫描：`loadVisionPlugins` → `registerToolEditors` → Session → MainWindow。

### Integration Points

- 第三方：Halcon 17.12、OpenCV、PCL、Open3D、ONNX Runtime、Qt 5.15.2、相机厂商 SDK（待定一家为首选）。
- 现场：Modbus/TCP/串口设备、可选 MES HTTP/文件、RemoteRelay 端口与防火墙脚本。
- 不集成：运控厂家完整 SDK（本阶段）、云授权、浏览器客户端。

### Data Storage & Privacy

- 路径：`exe/data/`（auth、log、prod、settings、node、solutions）；许可在 ProgramData；远程审计与中继日志按远程方案。
- 配置可加密；许可证与私钥隔离；远程 TLS + 指纹；日志禁止明文倾倒申请码/注册码/私钥。
- 账户口令哈希升级；生产图像属客户敏感数据，远程与 MES 需授权与最小化。

### Scalability & Performance

- 单机多工站串并行；多机靠 Station/Link 水平扩展，不传原图。
- 高速模式与远程子码流/拥塞降级保障 UI 与网络。
- 三维点云内存按三维方案拷贝语义；大点云需抽样显示。

### Potential Challenges

- 阶段对比文档过时导致排期误判——以本 PRD「文档口径」表为准。
- 调度语义（S1–S3）与真相机同时开工时的优先级冲突——P0 必须同时列进单机闸门。
- Halcon/OpenCV 覆盖不齐、Open3D 部分「未实现」——交付包要有能力矩阵。
- 评估中图像所有权（H2）、Halcon 句柄释放等未结论项——需在联调中单列验证任务。

---

## Milestones & Sequencing

### Project Estimate

整体为 **Large（约 4–8 人月量级，可按闸门切开交付）**。单机闸门目标约 **4–6 周**；多机约 **3–4 周**；远程固化与许可加固约 **2–3 周**；生产数据/MES/统计与 3D·AI 产线打磨并行滚动 **4–8 周**。

### Team Size & Composition

建议 **Medium 精简班组：3–4 人**——1 名平台/调度、1 名设备与通信（含相机 SDK）、1 名 HMI/远程/许可、1 名算法插件（2D/3D/ONNX，可兼）。不设大编制；文档与验收由同一班组维护。

### Suggested Phases

**Phase 0 — 文档与基线对齐（约 3–5 天）**

- Key Deliverables：本 PRD 评审通过；更新「阶段对比」过时表或加勘误；能力矩阵（工具 × 算法库 × 依赖）；选定首家相机 SDK。
- Dependencies：无代码冻结要求；不改业务源码亦可完成。

**Phase 1 — 单机可交付闸门（P0，约 4–6 周）**

- Key Deliverables：至少一家真相机接入；文件源回归；S1/S2/S3/U1 稳定性收口；Release 登录与口令；软许可启动/倒计时主路径；方案版本拦截；2D 主路径（采集→定位/检测→判定→通信）验收用例集。
- Dependencies：相机 SDK 开发包与样机；Halcon/OpenCV 按交付矩阵。
- **验收场景：单机。**

**Phase 2 — 多机 Station/Link（P1，约 3–4 周）**

- Key Deliverables：`node.json` / `deploy.json` 文档化安装；托盘与 Station/Link 拉起；运行方式方案 §14 六条；生产结果落库 MVP（批次 + OK/NG + 图路径）。
- Dependencies：Phase 1 单机循环稳定。
- **验收场景：多机。**

**Phase 3 — 远程控台与许可加固（P2，约 2–3 周）**

- Key Deliverables：按远程 §18 抽样 A1–A10；生产保护与工站格子状态；许可签发作业指导书；配置加密默认开。
- Dependencies：现场网或实验室多机；可选 Relay。
- **验收场景：远程。**

**Phase 4 — 生产数据、MES、报警与高速（P3，约 4–6 周）**

- Key Deliverables：单料追溯与还原；复检导出升级；良率报警；高速模式；MES 上传口 + 一种适配演示；CPK/GRR 可选。
- Dependencies：Phase 2 结果库 MVP。

**Phase 5 — 三维 / AI 产线打磨与台账预留（P4，滚动）**

- Key Deliverables：`3d-tools.md` / AI §11 验收固化；三维相机/手眼从占位到可选项；运控/IO **仅台账与文档**；混淆挂接按需。
- Dependencies：客户是否采购 3D/AI 依赖库。

### 分期验收总闸门（必须遵守）

| 闸门 | 场景 | 通过才进入下一闸门 |
|---|---|---|
| G1 单机 | 一台工控机 + 真相机（或借测）+ 文件源 | 设计/运行/复检主路径、许可、登录、调度语义与相机错误可诊断 |
| G2 多机 | ≥2 台角色为 server/client | Station/Link §14；部署表；汇总画面 |
| G3 远程 | 中控 + 被控（+ 可选中继） | 远程 §18 抽样；与生产保护联动 |

---

## Open Questions

1. 首家真相机 SDK 最终选 Hik、Hray 还是其它？触发模式与像素格式以哪条产线为准？
2. MES 第一适配客户与协议（HTTP JSON / 数据库 / 文件）？
3. 生产原图默认存满图还是缩略图 + 可配置满图？磁盘配额策略？
4. 软许可产品名是否始终固定 `VisionPlatform`，是否需要按工位模块授权（工具包 SKU）？
5. 三维显示是继续增强 HsImageCtrl 还是独立控件（见 `19-图像控件三维显示评估.md`）的最终裁定？

---

## 参考路径（只读）

- `VisionPlatform/README.md`
- `VisionPlatform/docs/architecture.md`、`3d-tools.md`、`15-工具插件方案.md`、`16-工站流程工具配置重构评估.md`、`versioning.md`
- `ImageProcessPlatform/docs/VisionPlatform项目评估.md`
- `ImageProcessPlatform/docs/视觉平台开发阶段对比.md`（需结合本 PRD 文档口径表）
- `ImageProcessPlatform/docs/许可方案.md`、`配置加密方案.md`、`代码混淆方案.md`
- `ImageProcessPlatform/docs/运行方式与多端共享方案.md`、`远程控制方案.md`
- `ImageProcessPlatform/docs/17-三维方案.md`、`18-AI方案.md`、`19-图像控件三维显示评估.md`

---

*本文件为产品需求文档，不构成源码变更。实现须另开任务 / PR，并对照各闸门验收。*
