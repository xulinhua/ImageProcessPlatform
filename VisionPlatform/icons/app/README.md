# 应用图标

| 文件 | 用途 |
|---|---|
| `app.ico` | 嵌入 exe（`VisionApp.rc`），资源管理器 / 任务栏 |
| `app.png` | Qt `setWindowIcon`（标题栏） |
| `app.svg` | 矢量源（可再导出） |

当前为蓝底渐变 + 白色「视觉之眼」标。替换后需**重新生成** `VisionApp`（ico 编进 exe），`CopyQt.bat` 会拷贝 png 到 exe 旁。

辅助脚本：`export_ico.ps1`（从高清 PNG 导出多尺寸 ico）。
