# HsImage

独立图像协议与显示控件方案，与 `VisionPlatform` 解耦开发，产物以第三方库形式接入平台。

## 工程一览

| 工程 | 类型 | 职责 |
|------|------|------|
| [HsCommData](HsCommData/README.md) | DLL | 几何 / 图像 / 叠加协议，无 Qt |
| [HsImageCtrl](HsImageCtrl/README.md) | DLL | Qt 图像窗口与 `ImageWidgetApi` 壳层 |
| [HsImageDemo](HsImageDemo/README.md) | EXE | 交互演示（ROI、叠层、阈值等） |
| [HsImageBench](HsImageBench/README.md) | EXE | 转换与对象相关性能台架 |

解决方案：`HsImage.sln`（VS2015 / 工具集 v140）。

## 依赖关系

```text
HsCommData  (无 Qt)
    ^
HsImageCtrl (Qt Widgets)
    ^
HsImageDemo / HsImageBench
```

## 构建

1. 用 VS2015 打开 `HsImage.sln`。
2. 配置 `Debug|x64` 或 `Release|x64`（亦可 Win32）。
3. 生成解决方案。产物输出到：

```text
HsImage\bin\<Platform>\<Configuration>\
  HsCommData.dll / .lib
  HsImageCtrl.dll / .lib
  HsImageDemo.exe
  HsImageBench.exe
```

### 环境约定

| 项 | 默认 |
|----|------|
| Qt | `C:\Qt\Qt5.9.9\5.9.9\msvc2015_64`（Win32 对应 `msvc2015`） |
| HALCON（Demo/Bench 可选） | `C:\Program Files\MVTec\HALCON-17.12-Progress` |
| OpenCV（Ctrl/Bench 可选） | `C:\opencv\build` |

可用环境变量 `QTDIR`、`HALCONROOT`、`OPENCV_DIR` 覆盖。

### 源码编码

中文 Windows 下源码按 **系统 ANSI/GBK** 保存（与 VisionPlatform 一致），**不要**对工程加 `/utf-8`。注释与 `fromLocal8Bit` 窄字面量均按此约定。

## 接入 VisionPlatform

编好后将对应配置的 DLL/LIB 拷到：

```text
VisionPlatform\third_party\HsCommData\bin|lib\<Platform>\<Configuration>\
VisionPlatform\third_party\HsImageCtrl\bin|lib\<Platform>\<Configuration>\
```

头文件同步到：

```text
VisionPlatform\third_party\HsCommData\include\HsCommData\
VisionPlatform\third_party\HsImageCtrl\include\
```

平台侧通过 `vision.props` / 工程引用链接 `HsCommData.lib`、`HsImageCtrl.lib`，运行时从 `third_party\...\bin` 拷贝 DLL。

## 相关文档

- [docs/绘制与屏蔽方案.md](docs/绘制与屏蔽方案.md) — 绘制会话与屏蔽涂抹设计
