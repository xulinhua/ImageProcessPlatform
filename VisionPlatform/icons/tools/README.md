# 图标资源（Lucide 成套 SVG）

来源：[Lucide Icons](https://lucide.dev/)（ISC 许可，与 MIT 同级），静态包 `lucide-static@0.469.0`。

## 目录

| 路径 | 用途 |
|---|---|
| `icons/tools/*.svg` | 工具树 / 工具库 |
| `icons/ui/*.svg` | 主工具栏与模式页签 |
| `icons/app/` | 应用图标 |
| `icons/vendor/lucide/` | 原始 Lucide 文件备份 |

构建时由 `CopyQt.bat` 拷到 `exe/icons/`。

## 工具图标

分类节点仍用类型图标；叶子工具优先加载 `tools/<toolName>.svg`，缺失时回退到类型图标。

### 类型 / 流程结构

| 文件 | Lucide 源 | 用途 |
|---|---|---|
| camera.svg | camera | 采集类 |
| preprocess.svg | image | 预处理类 |
| locate.svg | crosshair | 定位类 |
| detect.svg | scan-search | 检测类 |
| logic.svg | scale | 逻辑类 |
| comm.svg | cable | 通信类 |
| result.svg | badge-check | 结果处理类 |
| parallel.svg | columns-2 | 并行组 |
| if.svg | git-fork | 条件组 |
| branch.svg | git-branch | 支路 |

### 具体工具（toolName）

| 文件 | Lucide 源 | 颜色 |
|---|---|---|
| camera.device.svg | aperture | #1E88E5 |
| camera.file.svg | folder-open | #1E88E5 |
| preprocess.enhance.svg | contrast | #43A047 |
| preprocess.smooth.svg | waves | #43A047 |
| locate.template.svg | layout-template | #8E24AA |
| locate.find_line.svg | scan-line | #8E24AA |
| locate.find_circle.svg | circle-dot | #8E24AA |
| locate.caliper.svg | ruler | #8E24AA |
| detect.blob.svg | shapes | #E53935 |
| detect.texture.svg | grid-3x3 | #E53935 |
| logic.judge.svg | scale | #3949AB |
| comm.plc.svg | cpu | #00897B |
| result.merge.svg | merge | #FB8C00 |

## 主界面图标

| 文件 | 用途 |
|---|---|
| new / open / save | 方案 |
| stations / devices / comm | 工站·设备·通信 |
| run / start / stop / reset | 运行控制 |
| design / runtime / recheck | 顶栏模式 |

替换：直接覆盖同名 `.svg` 后重新编译（或拷到 exe 旁并重启）。缺失时工具树会回退到细线占位图或类型图标。
