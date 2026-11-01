# 通用视觉平台（VisionPlatform）

工业视觉主程序 + 工具/算法插件 + 本机节点服务 + 远程控台。  
技术栈：**VS2022（v143）+ C++17 + Qt 5.15.2**。方案：`VisionPlatform.sln`，启动工程 **VisionApp** → 产出 **`VisionPlatform.exe`**。

| 文档 | 内容 |
|---|---|
| 本文 | 环境、编译、运行程序、数据目录、账户、许可、远程与节点 |
| [docs/architecture.md](docs/architecture.md) | 领域模型、调度与插件加载（工程名有旧称，以本文与 `.sln` 为准） |
| [docs/versioning.md](docs/versioning.md) | 软件 / 模块 / 工具版本与方案字段 `platformVersion` |
| [docs/3d-tools.md](docs/3d-tools.md) | 三维工具清单与分期 |
| [docs/15-工具插件方案.md](docs/15-工具插件方案.md) | 工具 / 算法插件约定 |
| [`../docs/许可方案.md`](../docs/许可方案.md) | 许可设计；现场操作以本文「许可」为准 |
| [`../docs/远程控制方案.md`](../docs/远程控制方案.md) | 远程控台设计 |
| [`../docs/运行方式与多端共享方案.md`](../docs/运行方式与多端共享方案.md) | Station / Link / IPC |

---

## 1. 产出程序

| 工程 | 可执行文件 | 用途 |
|---|---|---|
| **VisionApp** | `VisionPlatform.exe` | 主界面：方案、设计/运行/复检、设备与通信、插件宿主 |
| **Vision.Station** | `Vision.Station.exe` | 本机工站服务：经 IPC 跑本机工站循环（可装 Windows 服务） |
| **Vision.Link** | `Vision.Link.exe` | 节点链路：CommFrame 收发；**不跑工站、不存原图** |
| **Vision.Control** | `RemoteMaster.exe` | 远程中控台 |
| **Vision.Controlled** | `RemoteClient.exe` | 远程被控端（可 `--service`） |
| **Vision.Relay** | `RemoteRelay.exe` | 配对 / NAT 中继（TCP 字节转发） |
| **Vision.LicenseRequest** | 同名 exe | 客户机：申请码 / 写入许可证 |
| **Vision.LicenseSign** | 同名 exe | 厂家机：签发注册码（勿随客户包分发） |
| VisionCoreTest | `VisionCoreTest.exe` | 方案 IO 自检 |
| Vision.ObfusTool | `Vision.ObfusTool.exe` | 混淆工具 |

静态库（不单独出 exe）：**Vision.Remote**（VRMT 协议）、**Vision.RemoteUi**、**VisionGrabFile**、**Plugin.VisionHalCom** / **Plugin.VisionCvCom** 等。

---

## 2. 仓库结构（按职责）

```text
VisionPlatform/
├── VisionApp / VisionUi / VisionToolUi / VisionWidgets   # 主程序与界面
├── VisionCore / VisionToolBase / VisionAlgToolBase       # 方案·工站·Job·工具 / 算法基类
├── VisionPluginAPI                                      # 插件入口（apiVersion=1）
├── VisionCommon / VisionCfgMgr / VisionCfgCrypt          # 公共、配置、加密
├── VisionAuth / VisionLog / VisionLang / VisionDb / VisionFile
├── VisionComm / VisionCalib / VisionLicense / VisionObfus / VisionRun
├── VisionIpc / IpcBase                                  # 本机共享内存与节点配置
├── VisionGrabFile                                       # 文件源采集（真相机 SDK 未接）
├── HsImageCtrl / HsCommData                             # 图像控件与公共图形类型
├── plugins/tools / plugins/algos                        # Tool / Algo 插件工程
├── Plugin.VisionHalCom / Plugin.VisionCvCom             # Halcon / OpenCV 插件公共库
├── Vision.Remote / Vision.RemoteUi                      # VRMT 协议与远程 UI
├── Vision.Control / Vision.Controlled / Vision.Relay    # 中控 / 被控 / 中继
├── Vision.Station / Vision.Link                         # 工站服务 / 节点链路
├── Vision.LicenseRequest / Vision.LicenseSign
├── third_party/                                         # Halcon、OpenCV、PCL、Open3D、sqlite…
├── samples/ icons/ lang/ docs/ tools/
├── bin/ obj/                                            # 输出与中间文件（vision.props）
├── *.props / CopyQt.bat / VisionPlatform.sln
```

顶层还有 `VisionKernel/`、`VisionDomain/`、`VisionTools/`、`Vision.LicenseAdmin/` 等目录，**多数未进当前 `.sln`**，不要当现役工程依赖。

解决方案筛选器大致为：Start、HMI、ClassLib、Tool / Alg 插件、CamSDK、Remote、License、HsImage。插件 DLL 生成到：

```text
bin\<Platform>\<Configuration>\plugins\tools\Plugin.Tool.*.dll
bin\<Platform>\<Configuration>\plugins\algos\Plugin.Alg.*.dll
```

主程序启动时扫描上述目录（`loadVisionPlugins`），再 `registerToolEditors` → Session → MainWindow。

---

## 3. 环境

| 项 | 说明 |
|---|---|
| IDE | Visual Studio 2022，平台工具集 **v143**，C++17 |
| Qt | **5.15.2**。环境变量 **`QTDIR`** 指向套件根目录：x64 用 `msvc2019_64`，Win32 用 `msvc2019`。`qt.props` → 上级 `ImageProcessPlatform/QtEnv.props` |
| Halcon | 17.12，仓库内 `third_party\Halcon`；`halcon.props` 只认相对路径 |
| OpenCV | `third_party\OpenCV`；`opencv.props` |
| PCL | `third_party\PCL`；属性表 **`third_party\PCL\pcl.props`** |
| Open3D | `third_party\Open3D`；属性表 **`third_party\Open3D\open3d.props`** |
| 推荐配置 | **x64 \| Debug** |

无 Halcon / 对应第三方时，相关 **Alg DLL 不会加载**（不存在「内置假算法」兜底）。

---

## 4. 编译与运行

1. 打开 `VisionPlatform.sln`
2. 启动工程选 **VisionApp**
3. 配置 **x64 | Debug**（或 Release）→ 生成 / F5
4. 输出根目录：`bin\<Platform>\<Configuration>\`

VisionApp 生成后会跑 **`CopyQt.bat`**：拷贝 Qt 运行库与插件、`icons/{app,tools,ui}`、`samples/` 下示例图，以及可用的 Halcon / PCL / Open3D 运行库到 exe 旁。链接前 `tools\free_exe.bat` 尝试释放被占用的 exe。

调试 PATH（工程内已配）需能找到 `QTDIR\bin` 与各第三方 `bin`。

可选参数：

| 参数 | 作用 |
|---|---|
| `--console` | 控制台模式（与托盘互斥），可本地启停工站 |

默认文件源指向 exe 旁 `samples/`；可在设备对话框改路径。工具树图标：`icons/tools/`；主程序：`icons/app/`；许可程序：`icons/license/`。

**说明**：`VisionWidgets` 由 VisionUi / VisionApp 以 **ProjectReference** 编入，不一定出现在 `.sln` 的 Project 列表里；全量生成时随引用构建即可。

---

## 5. 领域与界面（主程序）

领域主线：`Solution`（`*.vpsol`）→ `Station` → `Job` → `Tool`；方案还可含设备台账、通信通道、全局变量、UI 页、`platformVersion` 等（见 VisionCore）。

| 模式 | 做什么 |
|---|---|
| **设计态** | 资源树、Job 画布连线、执行本工具/本流程、`ToolDebugHost` 调参 |
| **运行态** | 工站循环出图与计数 |
| **复检** | 离线文件夹批量、结果表、导出 |

常见入口：工站 / 设备 / 通信 / 设置 / 流程配置；**帮助 → 版本管理…**、**帮助 → 关于**。

插件约定（摘要）：

- 每个 **Tool DLL** 与对应 **Algo DLL** 共用同一 `toolName`（小写点分，如 `locate.find_circle`）
- 宿主只依赖 **VisionPluginAPI** 与基类头；工具/算法子类头留在插件工程内
- 算法族：`Plugin.Alg.Hal.*`（Halcon）、`Plugin.Alg.Cv.*`（OpenCV）、`Plugin.Alg.Pcl.*` / `Plugin.Alg.Open3d.*`、`Plugin.Alg.Onnx.Infer.*`

能力现状（相对代码，非路线图承诺）：

| 已具备 | 仍弱 / 未接 |
|---|---|
| 多工站 / 多 Job、串并行调度字段、结果规则矩阵 | 真相机 / 控制卡 SDK（文件源可用） |
| 设备台账、串口 / TCP / MODBUS 等通信 | MODBUS 全功能码、Job 通信节点深化 |
| 本机设置中心、登录权限、版本查询、HsImage + ROI | 关闭到托盘等预留项 |
| 2D 工具链 + 大量 3D / ONNX 插件工程 | 部分 3D 工具仍为分期落地 |
| 设计态执行、Draft 冲突检查、运行态 + 复检 UI | FailSkip / NgContinue 等分支未完整 |

---

## 6. 数据目录

相对 **exe 目录**：

```text
exe/
  VisionPlatform.exe
  samples/                 # 示例图（构建拷贝）
  plugins/tools|algos/     # 插件 DLL
  icons/ …
  data/
    auth.db                # 用户与权限
    log.db / prod.db / verify.db / calib.db
    settings.json          # 本机设置
    node.json              # 本机节点角色（Station/Link）
    logs/
    solutions/             # 方案默认目录（*.vpsol）
```

方案旁可有 **`deploy.json`**（工站 id → 节点 id，多机部署用）。

许可证不在 exe 旁：`%ProgramData%\VisionPlatform\license\license.dat`。时间戳另有三处：`HKCU\Software\VisionPlatform\License`、`%ProgramData%\…\tick.dat`、`%LOCALAPPDATA%\…\tick.dat`。

方案保存时写根字段 `platformVersion`（当前为软件版本字符串，见 `VisionVersion.h`）；打开时读取，**暂不做兼容拦截**。详见 [versioning.md](docs/versioning.md)。

---

## 7. 账户与登录

空库首次启动写入内置账户，**默认口令为空**：

| 登录名 | 角色 |
|---|---|
| admin | 管理员 |
| engineer | 工程师 |
| operator | 操作员 |

用户与权限在 `data\auth.db`（`users` / `perm`）。

| 构建 | 登录行为 |
|---|---|
| **Debug** | `enterAdminForDev()`：不弹登录，管理员空口令并恢复开发权限 |
| **Release** | 先尝试 `autoLoginOperator()`；否则弹出 `LoginDlg` |

---

## 8. 许可

筛选器 **License** 下两个带界面程序。**私钥只在厂家电脑。**

| 程序 | 在哪跑 | 做什么 |
|---|---|---|
| `Vision.LicenseRequest.exe` | 客户电脑 | 打出申请码；粘贴注册码，验过后写许可证并更新三处时间记录 |
| `Vision.LicenseSign.exe` | 厂家电脑 | 解开申请码，签发注册码。**不写**客户的 `license.dat` |

- 客户程序一分钟内注册最多 50 次；超限禁用「注册」，窗口内最早一次超过一分钟后恢复；重启程序计数清零。
- 注册机启动密码当前为 `hoson123`；错则退出不可重试。旁路文件 **`sign.key`**（ECC 私钥十六进制）缺失则不能签发。客户安装包不要带注册机和 `sign.key`。

办理顺序：

1. 客户复制/导出申请码（或二维码）。前缀 `VP-A1-`，含产品名、硬件摘要、当前序号（无证时为 0）。
2. 注册机粘贴或导入 txt →「查看」→ 序号自动为当前 +1；备注编号默认 `issued`，不参与验机。
3. 选永久或到期日（`YYYY-MM-DD`）→「签发」。注册码前缀 `VP-R1-`，序号须大于申请码序号。
4. 客户粘贴/导入注册码 →「注册」。验签、产品名、硬件摘要通过且序号更大才覆盖许可证。

硬件摘要取 CPU、主板固件、系统盘。主程序启动查许可，失败可粘贴注册码；运行中约每 30 秒复查，失败后倒计时约 10 分钟，到点停工站。`Vision.Station` 开工站前后也会查。设计见 [`../docs/许可方案.md`](../docs/许可方案.md)。

---

## 9. 本机节点（Station / Link）

配置文件：`exe\data\node.json`（`NodeSettings`）。

| 字段要点 | 含义 |
|---|---|
| `role` | `standalone` / `server` / `client` |
| `stationEnabled` / `linkEnabled` | 是否拉起工站服务 / 链路服务 |
| 监听与对端 | server 监听、client 连接的 host/port 等 |

- **`standalone`**：不走多机 IPC 编排，主程序本地跑工站即可。
- **非 standalone**：映射共享内存（`VisionIpc` / `SharedFrames`），托盘可拉起 **Vision.Station** / **Vision.Link**。
- 方案旁 **`deploy.json`** 决定工站归属哪个 `nodeId`。

Station / Link 与远程控台协议分离：节点用 CommFrame；远程用 **VRMT**（见下节）。

---

## 10. 远程控台（VRMT）

| 角色 | 程序 |
|---|---|
| 中控 | `RemoteMaster.exe`（工程 Vision.Control） |
| 被控 | `RemoteClient.exe`（工程 Vision.Controlled） |
| 中继 | `RemoteRelay.exe`（工程 Vision.Relay，可选） |

协议库：**Vision.Remote**。帧头 magic **`VRMT`**，version **2**；通道含控制、键鼠、光标、主/子码流、剪贴板、文件、聊天等。分片用帧头 `FlagFin` / `FlagCont`。业务应答是独立报文类型（如 `TypeHelloAck`、`TypeHeartbeatAck`、`TypeFileAck`），**不是 TCP ACK**，也与预留未用的帧头 `FlagAck` 无关。

能力包括：局域网发现、认证与操作权、画面（JPEG / 可选 H.264）、文件与剪贴板、聊天、电源与隐私屏等。公网 ICE/STUN 等按方案后续再开。设计见 [`../docs/远程控制方案.md`](../docs/远程控制方案.md)。

---

## 11. 相关文档（上级仓库）

上级 `ImageProcessPlatform/docs/` 还有三维 / AI / 混淆 / 配置加密等方案文，与实现分期对照时查阅；**以本目录代码与本文为准**。
