# 通用视觉平台（VisionPlatform）

VS2015（v140）+ Qt 5.9.9 + C++11。方案文件：`VisionPlatform.sln`，启动工程 **VisionApp**，产出 `VisionPlatform.exe`。

- **使用**（本文）：环境、编译、运行、账户与数据目录
- **架构**：[docs/architecture.md](docs/architecture.md)（工程分层、领域模型、调度与版本流）
- **版本号约定**：[docs/versioning.md](docs/versioning.md)

---

## 使用

### 环境

| 项 | 说明 |
|---|---|
| IDE / 工具集 | Visual Studio 2015，平台工具集 **v140** |
| Qt | 5.9.9（msvc2015 / msvc2015_64），路径见 `qt.props`，可用环境变量 **`QTDIR`** 覆盖 |
| Halcon | 可选；`halcon.props` 探测 `HALCONROOT`。有 SDK 时编入真实后端，否则走 builtin |
| 推荐配置 | **x64 \| Debug** |

### 编译与运行

1. 打开 `VisionPlatform.sln`
2. 启动工程选 **VisionApp**
3. 配置 **x64 | Debug**（或 Release）→ 生成 / F5
4. 输出目录：`bin\<Platform>\<Configuration>\VisionPlatform.exe`  
   生成后脚本会拷贝 Qt 运行库、`samples/`（及可用的 Halcon DLL）到 exe 旁

默认文件源指向 exe 旁 `samples/`；也可在设备对话框中改文件夹。工具树图标在 `icons/tools/`，应用图标在 `icons/app/`（构建拷到 exe 旁，可直接替换同名文件）。

### 默认账户

空库首次启动会写入内置账户，**默认口令为空**（空字符串）：

| 登录名 | 角色 |
|---|---|
| admin | 管理员 |
| engineer | 工程师 |
| operator | 操作员 |

用户库路径：`exe\data\users.json`。

> 当前 `main.cpp` 开发入口会 `enterAdminForDev()` 直接以管理员进主界面；正式登录流程仍保留在 Auth 模块。

### 数据目录

一律相对 exe，**不使用** `%LOCALAPPDATA%`：

```text
exe/
  VisionPlatform.exe
  samples/          # 示例图（构建拷贝）
  data/
    users.json
    settings.json
    logs/
    solutions/      # 方案默认保存目录
```

方案文件扩展名：`*.vpsol`。保存时写入根字段 `platformVersion`（当前软件版本）；打开时读取，暂不做兼容拦截。详见 [versioning.md](docs/versioning.md)。

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
