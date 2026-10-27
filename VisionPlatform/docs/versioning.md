# 版本号约定

## 软件版本

改 [`VisionCommon/common/VisionVersion.h`](../VisionCommon/common/VisionVersion.h) 中的：

- `VISION_VERSION_MAJOR` / `MINOR` / `PATCH`
- 宏展开为 `VISION_VERSION_STRING`（如 `1.0.0`）

界面：**帮助 → 版本管理…**（`VersionDialog`，模块/工具明细）；**帮助 → 关于**（`AboutDialog`，品牌/简介/版本摘要/版权）。

## 模块版本

同文件中的 `VISION_MODULE_*_VERSION`。版本管理对话框「模块」表展示这些常量。

## 工具版本

- 接口：`ToolBase::version()`，写入 `ToolDesc.version`
- 默认 `"1.0.0"`；采集工具用 `VISION_MODULE_GRABFILE_VERSION`，算法基类工具用 `VISION_MODULE_ALGOBASE_VERSION`
- 某工具行为不兼容升级时，只改该工具的 `version()`，并在发布说明写一句

## 方案文件

保存 `*.vpsol` 时写入根字段 `platformVersion`（当前软件版本）。打开时读取但不做兼容拦截。

## 数据目录

一律相对 exe：`exeDir/data/`（含 `logs/`、`solutions/`、`users.json`、`settings.json`），不使用 `%LOCALAPPDATA%`。