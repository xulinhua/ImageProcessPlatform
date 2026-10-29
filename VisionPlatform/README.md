# 通用视觉平台（VisionPlatform）

VS2022（v143）+ Qt 5.15.2 + C++17。方案文件：`VisionPlatform.sln`，启动工程 **VisionApp**，产出 `VisionPlatform.exe`。

- **使用**（本文）：环境、编译、运行、账户、数据目录与许可
- **架构**：[docs/architecture.md](docs/architecture.md)（工程分层、领域模型、调度与版本流）
- **版本号约定**：[docs/versioning.md](docs/versioning.md)

---

## 使用

### 环境

| 项 | 说明 |
|---|---|
| IDE / 工具集 | Visual Studio 2022，平台工具集 **v143**，C++17 |
| Qt | 5.15.2。必须设置环境变量 **`QTDIR`** 为套件目录：x64 用 `msvc2019_64`，Win32 用 `msvc2019`。工程内不写默认路径 |
| Halcon | 17.12，已放在 `third_party\Halcon`（x64-win64）。`halcon.props` 只认这个相对路径 |
| 推荐配置 | **x64 \| Debug** |

### 编译与运行

1. 打开 `VisionPlatform.sln`
2. 启动工程选 **VisionApp**
3. 配置 **x64 | Debug**（或 Release）→ 生成 / F5
4. 输出目录：`bin\<Platform>\<Configuration>\`  
   主程序是 `VisionPlatform.exe`。客户注册程序是 `Vision.LicenseRequest.exe`，厂家注册机是 `Vision.LicenseSign.exe`。生成后脚本会拷贝 Qt 运行库、`samples/`（及可用的 Halcon DLL）到 exe 旁

默认文件源指向 exe 旁 `samples/`；也可在设备对话框中改文件夹。工具树图标在 `icons/tools/`，主程序图标在 `icons/app/`。两个许可程序的图标在 `icons/license/`：客户程序是绿色勾，注册机是橙色钥匙。

### 默认账户

空库首次启动会写入内置账户，**默认口令为空**（空字符串）：

| 登录名 | 角色 |
|---|---|
| admin | 管理员 |
| engineer | 工程师 |
| operator | 操作员 |

用户在 `exe\data\auth.db` 的 `users` 表里，角色权限在同一库的 `perm` 表里，一项权限一行。

> 当前 `main.cpp` 开发入口会 `enterAdminForDev()` 直接以管理员进主界面；正式登录流程仍保留在 Auth 模块。

### 数据目录

方案、设置、日志和用户库在 exe 旁：

```text
exe/
  VisionPlatform.exe
  samples/          # 示例图（构建拷贝）
  data/
    auth.db         # 权限
    log.db          # 日志，按种类和月份分表
    prod.db         # 生产数据，按批次分表
    verify.db       # 验证复核，按试验类型分表
    calib.db        # 标定记录。运行仍读方案里的当前结果
    settings.json
    logs/
    solutions/      # 方案默认保存目录
```

方案文件扩展名：`*.vpsol`。保存时写入根字段 `platformVersion`（当前软件版本）；打开时读取，暂不做兼容拦截。详见 [versioning.md](docs/versioning.md)。

许可证不放在 exe 旁。文件在 `%ProgramData%\VisionPlatform\license\license.dat`。时间记录有三处：`HKCU\Software\VisionPlatform\License`、`%ProgramData%\VisionPlatform\license\tick.dat`、`%LOCALAPPDATA%\VisionPlatform\license\tick.dat`。

### 许可

解决方案里单独的 **License** 筛选器放两个带界面的程序。私钥只在厂家电脑。

| 程序 | 在哪跑 | 做什么 |
|---|---|---|
| `Vision.LicenseRequest.exe` | 客户电脑 | 打出申请码；粘贴注册码，验过后写入许可证并更新三处时间记录 |
| `Vision.LicenseSign.exe` | 厂家电脑 | 解开申请码，签发注册码。不写客户的 `license.dat` |

客户程序一分钟内注册最多 50 次。超过后禁用「注册」并弹窗，最早一次超过一分钟后按钮恢复。关掉程序再开，这 50 次重新计数。

注册机启动要输入密码，当前写死为 `hoson123`。密码错误直接退出，不能重试。私钥文件 `sign.key` 放在注册机旁边，内容是 ECC 私钥的十六进制。没有这个文件就不能签发。客户安装包不要带注册机和 `sign.key`。

办理顺序：

1. 客户程序复制或导出申请码，也可以点「二维码」弹出窗口拍照。前缀 `VP-A1-`。里面有产品名、本机硬件摘要和当前序号。还没有许可证时序号是 0。序号变了，申请码也会变。
2. 注册机把申请码贴进去，或导入 txt，再点「查看」。摘要单独一行。序号框自动填成当前序号加 1。编号是厂家自己填的备注，默认 `issued`，只要求非空，不参与验机器。
3. 选「永久注册」，或选到期日。日期可以用日历，也可以手写，必须是合法的 `YYYY-MM-DD`。
4. 点「签发」。注册码可以复制、导出 txt，或点「二维码」拍照。前缀 `VP-R1-`。序号必须大于申请码里的当前序号。
5. 客户把注册码贴进注册程序，或导入 txt，再点「注册」。验签、产品名、硬件摘要通过，且序号更大，才覆盖许可证，并把三处时间记录写成当时的本机时间。

txt 里就是这一串码。导入只填入输入框。二维码窗口不另存图片。

硬件摘要只取 CPU、主板固件、系统所在盘。主程序启动时也会检查许可，失败可以粘贴注册码。运行中大约每 30 秒复查，失败后倒计时 10 分钟，到点只停工站。`Vision.Station` 开工站前和运行中也会查。设计说明见 [`../docs/许可方案.md`](../docs/许可方案.md)，现场操作以本节为准。

### 界面模式

| 模式 | 做什么 |
|---|---|
| **设计态** | 资源树、Job 画布连线、执行本工具 / 本流程、打开 `ToolDebugHost` 调参 |
| **运行态** | 工站循环出图与计数 |
| **复检** | 离线文件夹批量、结果表、导出；可跳回设计态调试 |

常见菜单：工站 / 设备 / 通信 / **设置** / 流程配置；**帮助 → 版本管理…**（模块与工具明细）；**帮助 → 关于**。

### 能力速览

| 已具备 | 仍弱 / 占位 |
|---|---|
| 多工站 / 多 Job 方案 JSON；串行/并行 `jobSchedule` | 真相机 SDK（文件源已可用） |
| 设备三类台账（相机 / 控制卡 / PLC）+ 遍历\|管理 | 控制卡真实 SDK |
| 方案级通信管理（串口枚举、TCP、MODBUS 03/06） | 完整 MODBUS 功能码 / Job 通信节点大改 |
| 本机设置中心（左导航：常规/界面/路径/日志入口） | 关闭到托盘（项已预留，灰显） |
| Job 画布数据连线、工具组 / If、结果规则矩阵 | 触发模式字段已保存，调度尚未全用 |
| 设计态执行工具/流程、Draft 冲突检查、`ToolDebugHost` | FailSkip / NgContinue 运行时分支未完整 |
| 运行态 + 复检批量 UI | |
| 登录权限模型（含 `PermEditComm`）、版本查询、HsImageWindow + ROI | Halcon 依赖本机 SDK；无时走 builtin |
| 插件式工具注册（AlgoBase / Halcon / GrabFile / 编辑器） | |

更细的实现对照见仓库文档 [`../docs/09-方案与流程.md`](../docs/09-方案与流程.md) §9.9。

### 文档索引

| 文档 | 内容 |
|---|---|
| [docs/architecture.md](docs/architecture.md) | 工程分层、领域模型、调用链、版本流 |
| [docs/versioning.md](docs/versioning.md) | 软件 / 模块 / 工具版本与 `platformVersion` |
| [docs/ui-mockups/](docs/ui-mockups/) | 复检、设置中心等界面示意图 |
| [`../docs/`](../docs/) | 方案与流程、Tool 综合体、PlantUML 图源 |
| [`../docs/10-基础设施与插件方案.md`](../docs/10-基础设施与插件方案.md) | 日志、数据库、文件、权限、登录、插件（规划草案，待确认） |
| [`../docs/许可方案.md`](../docs/许可方案.md) | 许可设计。现场怎么注册、签发，以本文「许可」一节为准 |
