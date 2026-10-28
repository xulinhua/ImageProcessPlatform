# VisionPlatform 项目评估

评估日期：2026-10-27。对象是仓库根下的 `VisionPlatform`，以及同级的 `HsImage`。依据当前源码和工程文件，没有重新编译，也没有跑界面。

## 1. 结论

平台骨架已经能往下做：方案、工站、流程、工具分层清楚，工具和算法按 DLL 拆开，设计、运行、复检三条路径都有代码。版本号是 `1.0.0`。仓库里约 60 个 `vcxproj`，其中插件 36 个（工具 18、算法 18）。

离稳定交付还差一截。问题不集中在某一个类，而是分布在构建、调度、插件、界面和文档上。下面按层列出。同一层里，先写会影响结果或编不过的项。

条目标题带删除线的，表示该项已经改过。标题里只划掉一部分的，表示只做完划掉的那半。其余标题仍是待处理问题。

## 2. 分层对照

| 层 | 主要目录 | 这一层现在的状态 |
|---|---|---|
| 构建与工程 | `*.vcxproj`、`qt.props`、`halcon.props`、`opencv.props` | ~~工具链与 Qt 路径不一致~~ 已改为 v143、C++17、Qt 5.15.2，路径来自环境变量 `QTDIR`。~~Halcon 探测本机安装目录~~ 已改为 `third_party\Halcon` |
| 调度与领域 | `VisionCore` | 工站 / 流程 / 工具模型可用；失败策略、触发、连线与执行树没有收口 |
| 配置与持久化 | `VisionCfgMgr`、`ToolCfg`、`*.vpsol` | 能读写方案；版本不拦截，配置字段和运行语义有缺口 |
| 插件与算法 | `plugins/`、`VisionAlgHal`、`VisionAlgCv` | 一工具一 DLL 已落地；Halcon / OpenCV 覆盖不齐，公共代码被各 DLL 各编一份 |
| 采集、通信、标定 | `VisionGrabFile`、`VisionComm`、`VisionCalib` | 文件采集可用；相机 SDK 未接；通信和标定有类型，现场能力不完整 |
| 界面与运行循环 | `VisionUi`、`VisionToolUi`、`VisionApp` | 三种工作区都在；停止循环会卡住界面线程；若干源文件过大 |
| 权限、日志、数据 | `VisionAuth`、`VisionLog`、`VisionDb` | 登录和 SQLite 日志有实现；口令哈希偏弱，Debug 会绕过登录 |
| 图像库 | `HsImage`、`third_party/HsImageCtrl` | 控件能用；源码和预编译库两套入口，窗口实现过大 |
| 文档与仓库 | `README.md`、`VisionPlatform/docs`、Git 远程 | 索引指向不存在的文件；GitHub 上的 `main` 仍是空的 |

## 3. 构建与工程

### ~~B1. 文档里的工具链和工程文件不是同一套~~

已解决（2026-10-27）。工程统一为 VS2022、v143、C++17、Qt 5.15.2。Qt 安装路径不写进工程，只读环境变量 `QTDIR`；未设置时仓库根目录的 `QtEnv.props` 会在编译前报错。x64 指向 `msvc2019_64`，Win32 指向 `msvc2019`。关于对话框、版本对话框、`README.md` 和插件方案开头已改成同一套说明。

Qt 官方没有 v143 预编译包，套件名是 msvc2019。BondVision 的 `cmake/Bondvision.props` 也是用 v143 工程去链接 `msvc2019_64`。本次没有重新编译解决方案。

### ~~B2. Halcon 缺失时，公共插件工程直接失败~~

已解决（2026-10-27）。Halcon 17.12 的头文件、x64 导入库和运行库拷到 `VisionPlatform\third_party\Halcon`，做法对齐 8601 把 `halconlib\halcon17` 放在工程旁边。`halcon.props` 只用这个相对路径，不再探测 `C:\Program Files\MVTec`，也不读环境变量 `HALCONROOT`。`VisionApp` 的调试路径和 `CopyQt.bat` 同样指向该目录。`HsImageDemo`、`HsImageBench` 的 Halcon 路径，以及 `HsImageCtrl`、`HsImageBench` 的 OpenCV 路径，改为仓库里已有的 `third_party`。

本机 `HALCON-17.12-Progress` 没有 `lib\x86sse2-win32`，所以只拷了 x64。Win32 编 Halcon 插件时，`Plugin.VisionHalCom` 仍会报错，提示缺 `lib\x86sse2-win32\halconcpp.lib`。本次没有重新编译解决方案。

### B3. ~~没有测试工程~~，也没有 CI

测试工程已加（2026-10-27）。`VisionPlatform.sln` 的 `Test` 文件夹下是控制台工程 `VisionCoreTest`，不引用 Qt 和 Halcon。它把空方案、一份最小方案（一个工站、一个流程、一个工具、一条输入连线）写到临时文件再读回，并检查不存在的路径和无法当文件打开的路径。失败时进程返回非 0。启动工程仍是 `VisionApp`。调度语义（S1、S2、S3）还没定，没有写进这个程序。

CI 还没有。持续集成至少要编 x64 Release 的 `VisionApp` 和 `VisionCoreTest`。`VisionApp` 仍需要本机已设置 `QTDIR`。

### B4. ~~Halcon 公共算法代码被编进多个 DLL~~，OpenCV 仍是各编一份

Halcon 侧已改（2026-10-27）。删掉 `HalAlgoHelpers.cpp`。取灰度的 `hsImageToGray` 放进 `Plugin.VisionHalCom`，只编一次。失败输出是 `AlgToolOut::fail`。平滑、增强、滤波自己做 `ReduceDomain` 和整图写回。Blob、Texture 自己打包连通域。每个 Halcon 插件只编自己的 `Hal*.cpp`，并链接 `Plugin.VisionHalCom`。

`VisionAlgCv\CvAlgoHelpers.cpp` 仍被 OpenCV 插件各自 `ClCompile`。`VisionAlgHal`、`VisionAlgCv` 仍然没有自己的 `vcxproj`。

## 4. 调度与领域模型

代码在 `VisionCore`。流程配置类型是 `JobCfg`，工具配置类型是 `ToolCfg`。

### S1. 失败策略三项都能保存，运行只处理停工站

`JobEnums.h` 里有 `FailStopStation`、`FailSkip`、`FailNgContinue`。流程对话框三项都会写入方案。`Job.cpp` 和 `Station.cpp` 里的判断只比较 `FailStopStation`。

`Station::tick` 的并行分支里，某个流程失败只会把本轮 `sr.ok` 标成 false，不会按策略取消其余流程。停工站的 `break` 只写在串行分支。

影响：界面上选「跳过」或「NG 时继续」，重新打开方案仍能看到该选项，运行行为和「停工站」相同，或在并行时根本不停止后续流程。

处理：为三种策略分别写明串行、并行、工具组、If 下的行为，并在 `Job::run` 和 `Station::tick` 里实现。没有实现的选项不要出现在可保存的下拉框里。

### S2. 触发方式在混合流程里被收成连续跑

`StationLoop::resolveContinuous` 的规则是：

- 有文件夹采集，整站连续跑。
- 全部是循环触发，连续跑。
- 循环和等待类混在一起，注释写「以连续为主，等待类由上游/手动另议」，代码直接 `return true`。
- 没有任何循环、也没有文件夹采集时，才等 `pulse`。

影响：一个工站里既有循环流程又有软件触发流程时，等待类流程也会被循环带着跑。配置里的触发字段不能单独决定这一条流程。

处理：触发按流程判断，不要收成工站上的一个布尔值。文件夹采集只推进那一条采集流程。

### S3. 执行树和工具连线仍会互相改写

`ToolCfg` 注释写输入连线 `links` 是权威。`JobCfg::links` 注释写它是缓存，只由 `rebuildLinksCache` 写入。

同时 `JobCfg::rebuildLinksFromTree` 仍在 `JobCfg.cpp` 里被多次调用。它根据执行树 `root` 发出一组连线，再和旧连线合并：保留用户拖过的边，给新工具补默认图像输入。树结构一变，连线会被补写。

画布坐标还在 `ToolCfg` 上（`canvasX` / `canvasY` / `canvasPlaced`），注释写真源要迁到 `JobLayout`，当前仍是过渡字段。`ToolLink::routeCustom` 也标了同样的过渡。

影响：改树、改画布、改工具连线，三条路径都能碰到同一批边。出了「线丢了」或「多了一条自动线」，要同时看树、工具 `links` 和 Job 缓存。`Job.cpp` 约 1575 行，`JobCfg.cpp` 约 1541 行，改一处容易漏另一处。

处理：定一条规则并写进 `JobCfg` 的注释和保存逻辑。建议工具上的输入边是唯一持久真源，执行树只决定顺序和 If / 并行，不再调用 `rebuildLinksFromTree` 去补边。旧方案打开时做一次迁移。补边若仍需要，只在「新建工具且该输入为空」时做，并记一条日志。

### S4. 工站锁包住整轮运行

`Station::tick(Solution*, ThreadPool*)` 一进入就持有 `m_mutex`，直到本轮全部流程结束。并行时在持锁状态下把 `job->run` 丢进线程池，再 `wait`。注释写明持锁期间禁止 rebuild。

影响：这一轮算法没跑完，需要这把锁的界面或重载就会等。工作线程若再调用会锁 `Station` 的函数，会和正在 `wait` 的 `tick` 对上。本次没有把 `Job::run` 的每条回调追完，改调度前要沿调用链确认。

处理：锁只保护「替换 Job 对象」和「读写配置指针」。算法执行使用本轮开始时复制出来的配置快照。替换方案前先停 `StationLoop` 并等线程退出。

### S5. `ToolCfg` 用引用成员做字段别名

`ToolCfg` 同时有嵌套的 `info` / `param`，又有 `id`、`params`、`rois` 等引用成员指到这些嵌套对象里。拷贝构造和赋值是手写的，引用必须绑回本对象。

影响：以后若有人补移动构造、放进需要重分配的容器时漏了绑定，字段会指到已经失效的嵌套成员。这类错误表现为参数写到别的工具上，不容易从调用处看出来。

处理：对外只保留 `info` / `links` / `param`。旧的扁平名字做成函数，不要做成引用成员。

## 5. 配置与持久化

### C1. 方案版本只记录，不拦截

保存 `*.vpsol` 时写入 `platformVersion`（当前 `VISION_VERSION_STRING`，即 `1.0.0`）。`docs/versioning.md` 写明打开时读取，不做兼容拦截。

影响：格式以后一变，旧文件会按新代码去读，缺字段时走默认值，用户看到的是「打开成功但流程变了」，而不是「这个文件版本不支持」。

处理：文件里写独立的格式号。读到不认识的格式号就拒绝打开并说明原因。能迁移的版本写明迁移函数，迁移完再进入编辑。

### C2. 运行态和配方还挤在同一份工具记录里

`ToolCfg` 上 `runtime` 不落盘，这是对的。`canvasX` / `canvasY` 仍和配方放在一起，保存时再写入布局。结果规则 `resultRules` 在 `JobCfg` 上，`Job::run` 里会按 `resultFromRuleId` 选用一条规则。

影响：布局、连线折线、配方参数的保存边界还在过渡。画布改动和参数改动容易一起把方案标脏，也容易在「只想保存参数」时把折线写坏，或反过来。

处理：持久化分成三块并在 `VisionCfgMgr` 里分函数写：工具配方（身份、参数、ROI、设备）、工具输入边、流程布局（坐标和折线）。运行态继续不落盘。

### C3. 配置模块过大，读写规则难查

`VisionCfgMgr.cpp` 约 1454 行，负责方案、用户和本机设置。`MainWindow::saveSolutionFile` 在保存失败时会弹出警告，这条路径是有反馈的。

影响：以后加字段时，加载默认值、保存、草稿对比（`RecipeDraft.cpp`）要改三处，漏一处就会出现「界面改了，重开方案又回去」。

处理：按方案、工站、流程、工具拆保存函数。每个新增字段配一条「写入后再读回」的检查，放进 B3 的核心测试。

## 6. 插件与算法

### P1. 一工具一 DLL 已经落地，文档仍按旧结构写

`plugins/tools` 有 18 个工具工程，`plugins/algos` 有 18 个算法工程。找线工具的注册名是 `locate.find_line`，参数表由 `ParamField` 生成。这和插件方案第 1 节的目标一致。

`docs/15-工具插件方案.md` 第 2 节仍写：找圆、找线、Blob 的壳在 `VisionTools.Base.dll`，Halcon 实现在 `VisionAlgo.Halcon.dll`。当前树里没有这两个工程名。

影响：按第 2 节去找注册表单例和「大包 DLL」会找错地方，拆分工作会被再做一遍。

处理：第 2 节改成当前目录。只保留尚未做到的条目，例如参数页是否还有按工具名写死的编辑器。

### P2. Halcon 与 OpenCV 的算法覆盖不齐

Halcon 侧能看到 Blob、滤波、平滑、增强、找圆、找线、卡尺、模板、纹理、合并、标定板、畸变校正。OpenCV 侧能看到 Blob、平滑、增强、找圆、标定板、畸变校正。找线、卡尺、模板、纹理、滤波、合并没有对应的 `Plugin.Alg.Cv.*`。

工具壳通过 `algLib` 选择算法库。选了 OpenCV 但没有该 DLL 时，工具执行会直接返回，界面显示未登记算法。这是插件方案里规定的行为。

影响：同一套工具在两套算法库上的能力表不一致。方案从 Halcon 改到 OpenCV 时，一部分工具会从「能跑」变成「未登记」，方案文件本身看不出缺了哪个 DLL。

处理：做一张工具名 × 算法库的对照表，放进版本对话框或启动日志。打开方案时，若 `algLib` 没有对应 DLL，在资源树上标出来，不要等到运行那一拍。

### P3. 没有相机 SDK，设备采集只会走文件

`CameraDeviceTool::execute` 在 `cameraBackend != CameraBackendFile` 时直接返回「相机 SDK 未接入」，并提示改绑文件源。曝光、增益参数写在 `defaultParams` 里，注释说明等 SDK 接入后使用。控制卡没有对应的采集实现。

影响：设备台账里可以登记非文件相机，运行结果是明确的失败文案，不会出图。现场若以为台账登记完成就等于能采图，会误判成算法问题。

处理：设备对话框里，SDK 未接入的后端不可选，或保存时标成不可运行。接入 SDK 时单独做相机插件，不要把 SDK 调用写进 `FileGrabber`。

### P4. 标定和通信工具在，现场协议不完整

工具工程里有 N 点、旋转中心、标定板、映射位姿、畸变校正，以及通信工具。`VisionComm` 有串口、TCP 和 Modbus 读写接口（线圈、离散、保持寄存器、输入寄存器的读写声明都在 `Modbus.h`）。README 写完整 Modbus 功能码和 Job 通信节点仍不完整。

影响：界面能配出标定和通信流程，不代表已经覆盖现场的功能码、从站异常和断线重连。

处理：给通信工具列一张「已实现的读写」表，和界面上的操作一一对应。没实现的功能码不要出现在可保存参数里。

## 7. 界面与运行循环

### U1. 停止运行循环会在界面线程里等待

`StationLoop::stop` 在线程可加入时调用 `join`。`MainWindow::clearLiveStations` 在界面侧调用 `slot->loop.stop()`，发生在关闭运行槽、切换方案这一类路径上。`requestStop` 本身不阻塞，`stop` 会一直等到当前 `tick` 结束。

影响：当前拍若卡在算法或采图上，界面在这一拍结束前不能再响应。用户会以为程序卡死。

处理：界面只调用 `requestStop`。线程真正退出后用排队调用通知界面，再删除 `Station`。不要在界面线程 `join`。

### U2. 运行循环放在界面工程里

`StationLoop` 位于 `VisionUi/runtime`，直接调用 `Station::tick`。连续、等待、文件夹采集的判断也写在这里。`VisionCore` 里没有这个循环。

影响：没有界面就无法复用同一套节拍规则。headless 或测试要链 `VisionUi`，或者把规则再抄一份。

处理：循环线程和触发判断移到 `VisionCore` 或单独的运行库。界面只负责启动、停止和显示 `StationRunRst`。

### U3. 主界面和画布文件过大

| 文件 | 行数（约） | 里面叠在一起的事 |
|---|---|---|
| `VisionUi/workspace/JobCanvas.cpp` | 3070 | 节点、连线、折线、交互 |
| `VisionUi/main/MainWindow.cpp` | 1665 | 菜单、方案开关、运行槽、模式切换 |
| `VisionUi/dialog/JobDlg.cpp` | 1549 | 流程结构、失败策略、结果规则 |
| `VisionUi/workspace/DesignWorkspace.cpp` | 1400 | 资源树、画布、试跑 |
| `VisionToolUi/debug/ToolDebugHost.cpp` | 1369 | 参数、ROI、单工具运行 |

影响：改画布交互容易碰到试跑和保存。代码审查很难只看一个职责。

处理：下次改这些文件时按上表的职责拆出类型。不要为拆分单独停功能开发。

### U4. Debug 启动不登录

`VisionApp/app/main.cpp` 在 `_DEBUG` 下调用 `ensureBuiltinEmptyPasswords`、`enterAdminForDev`、`resetDevPerms`，不弹登录框。Release 才走自动登录操作员或登录对话框。README 把推荐配置写成 x64 Debug，并写明默认口令为空。

影响：拿 Debug 目录去现场时，没有登录，并且会把内置空口令写进 `data/users.json`。

处理：现场和安装包只用 Release。Debug 绕过登录只保留在本机开发宏下，不要改已经存在的正式用户库。

## 8. 权限、日志与数据

### A1. 口令是单次 SHA256，PBKDF2 没有被调用

`AuthMgr::hashPassword` 计算 `SHA256(salt || password)`。`VisionAuth/user/Pbkdf2.cpp` 里有 `pbkdf2HmacSha256`，仓库内没有调用它的代码。

影响：用户库泄露后，空口令和短口令容易被算回来。迭代次数无法提高，因为正式路径没有走 PBKDF2。

处理：新口令改用已有的 PBKDF2，并在用户记录里记下算法和迭代次数。旧的单次 SHA256 在登录成功后升级。Debug 空口令不要写入正式库。

### A2. 日志进了 SQLite，方案结果没有统一的结果库

`VisionLog` 通过 `SqliteDatabase` 写 `vision.db`。`VisionDb` 提供的是通用打开、执行接口。流程 OK / NG、节拍、图像路径没有看到独立的结果表。

影响：现场要追溯某一拍为什么 NG，只能翻日志文本或方案旁路目录，不能按工站和帧号查询。

处理：先定要留的字段（时间、方案、工站、流程、帧号、OK/NG、摘要、图像路径），用 `VisionDb` 写一张结果表。日志继续负责诊断，不代替结果查询。

### A3. 数据全部放在 exe 旁边

README 规定不使用 `%LOCALAPPDATA%`。`users.json`、`settings.json`、`logs/`、`solutions/` 都在 exe 旁的 `data/`。

影响：程序装在只读目录时用户库和方案写不进去。多用户共用同一份 exe 时会共用同一份用户库。

处理：保持「默认在 exe 旁」可以，设置里已经有路径页时，把用户库、方案、日志的实际路径写进 `settings.json` 并允许改到可写目录。写失败时沿用现在的保存失败提示，不要当成保存成功。

## 9. 图像库

### H1. 源码工程和平台里的预编译库是两套入口

`HsImage.sln` 含 `HsCommData`、`HsImageCtrl` 等源码。`VisionPlatform` 通过 `third_party/HsImageCtrl`、`third_party/HsCommData` 使用头文件和库。`HsImageCtrl/display/HsImageWindow.cpp` 约 4630 行。`HsImage/image_widget` 下还有另一份窗口实现，`HsImage/.gitignore` 写明 Windows 以 `HsImageLib` 为准、不入库 Linux 对照工程。

影响：改了 `HsImage.sln` 里的控件，平台仍在用 `third_party` 里的旧 DLL，直到有人重新拷贝库。两份窗口代码会让缺陷修在一边、另一边还在。

处理：以 `HsImage.sln` 为源码真源。`third_party` 里的库注明对应的 HsImage 提交或版本号。平台工程不要再维护第二份窗口源码。

### H2. 图像对象跨拍传递的所有权还没有在评估里被运行验证

`CameraDeviceTool` 把 `FileGrabber::grab` 的 `HsImage` 按值放进 `ToolOut`。界面显示用的是运行结果里的图。本次没有追完「下一拍清空缓存后，界面是否仍拿着上一拍的缓冲」。

影响：若显示侧只保存了指针或视图，连续运行时会出现花屏或读到已释放内存。这项需要一次连续跑图才能定论，这里先列为待验证，不把它写成已经确认的缺陷。

处理：为 `ToolOut.image` 写明是独立缓冲还是视图。跨线程交给界面的那一份必须能在下一拍覆盖缓存后继续显示。

## 10. 文档与仓库交付

### D1. 文档索引指向仓库里没有的文件

`README.md` 链到仓库根的 `docs/09-方案与流程.md`、`docs/10-基础设施与插件方案.md`。`docs/architecture.md` 链到 `docs/Tool综合体-边界与调用.md`。这些路径在当前仓库中不存在。

`docs/16-工站流程工具配置重构评估.md` 编码损坏，正文无法当实施依据。它描述的「连线只在 Job 上」也已经落后于现在的 `ToolCfg::links`。

影响：新人按 README 找不到产品说明，或按 16 号文档去改已经迁走的字段。

处理：索引只保留仓库内存在的文件。16 号文档用 UTF-8 按当前 `ToolCfg` / `JobCfg` 重写，或从索引中撤下。

### D2. 远程仓库还没有代码

GitHub 仓库 `xulinhua/ImageProcessPlatform` 在本次评估时 `main` 仍为空，`size` 为 0。两个超过 100 MB 的 OpenCV Debug DLL（约 106 MB、131 MB）已用 Git LFS 记录在本地提交里。两个 Release DLL（约 54 MB、76 MB）在普通 Git 对象里。此前推送在代码包约 216 MB 发出后出现 `curl 56` / `errno 10054`（连接被重置）。

影响：别人克隆远程仓库看不到源码。LFS 对象即使已上传，没有 `main` 分支上的指针，克隆也拿不到这两个 DLL。

处理：在能稳定上传的网络下执行：

```bat
git -c http.version=HTTP/1.1 -c http.postBuffer=524288000 push -u origin main
```

成功时输出包含 `main -> main`。现场若不用 Debug 版 OpenCV，可只保留 Release，减小 LFS 体积。

## 11. 建议顺序

1. ~~B1~~、~~B2~~ 已解决。B3 只完成 ~~测试工程~~，CI 以后再说。
2. 收口 S1、S2：失败策略和触发方式要么执行，要么不让保存。
3. 收口 S3：工具输入边作为唯一持久真源，执行树不再回写连线。用 C3 的读写测试锁住。
4. 处理 U1、S4：界面不 `join` 运行线程，工站锁不包住整轮算法。
5. 处理 A1、U4：口令升级到 PBKDF2，现场包不用 Debug 入口。
6. 补 P2 的能力对照，打开方案时标出缺少的算法 DLL。
7. 修 D1 的文档索引，完成 D2 的推送。

## 12. 本次没有下结论的部分

- `ToolOut` 图像在下一拍之后是否仍被界面安全持有（见 H2）。
- Halcon 句柄在算法异常时是否都释放。
- 运行中打开另一份方案时，旧 `Station` 的释放顺序是否总能赶在下一拍之前。
- 各算法的精度和节拍。
