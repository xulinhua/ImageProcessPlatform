# HsImageBench

性能与转换台架：在独立线程里测图像生成、常见格式转换，以及 `HsObject` 等相关路径的耗时。用于对比本地实现与（可选）HALCON / OpenCV，**不做完整业务 UI**。

## 职责

- **转换页（ConvertBench）**：生成灰度/彩色测试图；循环测转换耗时；可限制「仅转 HALCON」等路径
- **对象页（ObjectPage / ObjectBench）**：围绕 `HsObject` 等协议对象的构建与操作计时
- 主窗口聚合上述页面，结果输出到文本区

## 目录结构

```text
HsImageBench/
  main.cpp
  MainWindow.h / .cpp     主界面与线程调度
  ConvertBench.h / .cpp   图像转换台架
  ObjectBench.h / .cpp    对象相关台架
  ObjectPage.h / .cpp     对象页 UI
  HsImageBench.vcxproj
```

## 依赖

| 依赖 | 说明 |
|------|------|
| HsImageCtrl / HsCommData | 显示与协议；链接同目录 `.lib` |
| Qt Widgets | 同 HsImageCtrl |
| HALCON（可选） | 检测到头文件时参与对比路径 |
| OpenCV（可选） | 检测到 `opencv2/core.hpp` 时启用 |

OpenCV 使用 `VisionPlatform\third_party\OpenCV`。x64 链接 `hsvisbasall64(d).lib`，Win32 链接 `hsvisbasall(d).lib`。

## 构建与运行

1. 生成解决方案（依赖顺序同 Demo）。
2. 运行：

```text
HsImage\bin\<Platform>\<Configuration>\HsImageBench.exe
```

生成后事件会拷贝 Qt（及存在时的 HALCON）DLL 到输出目录。

## 使用提示

- 大图 / 高循环次数时注意工作线程与 UI 线程分工，避免在界面线程里跑重循环。
- 未安装 HALCON/OpenCV 时仍可编译运行，对应对比项不可用或走本地路径。
- 本工程侧重计时与正确性抽查，交互深度不如 [HsImageDemo](../HsImageDemo/README.md)。

## 相关

- 协议库：[../HsCommData/README.md](../HsCommData/README.md)
- 显示控件：[../HsImageCtrl/README.md](../HsImageCtrl/README.md)
