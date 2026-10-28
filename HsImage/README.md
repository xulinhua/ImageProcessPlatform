# HsImage

独立图像协议与显示控件方案，与 `VisionPlatform` 解耦开发，产物以第三方库形式接入平台。

## 工程一览

| 工程 | 类型 | 职责 |
|------|------|------|
| [HsCommData](HsCommData/README.md) | DLL | 几何 / 图像 / 叠加协议，无 Qt |
| [HsImageCtrl](HsImageCtrl/README.md) | DLL | Qt 图像窗口与 `ImageWidgetApi` 壳层 |
| [HsImageDemo](HsImageDemo/README.md) | EXE | 交互演示（ROI、叠层、阈值等） |
| [HsImageBench](HsImageBench/README.md) | EXE | 转换与对象相关性能台架 |

解决方案：`HsImage.sln`（VS2022 / 工具集 v143 / C++17）。

## 依赖关系

```text
HsCommData  (无 Qt)
    ^
HsImageCtrl (Qt Widgets)
    ^
HsImageDemo / HsImageBench
```

## 构建

1. 用 VS2022 打开 `HsImage.sln`。
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
| Qt | 环境变量 `QTDIR`。x64 指向 `msvc2019_64`，Win32 指向 `msvc2019` |
| HALCON（Demo/Bench，x64） | `VisionPlatform\third_party\Halcon` |
| OpenCV（Ctrl/Bench） | `VisionPlatform\third_party\OpenCV` |

Qt 只读环境变量 `QTDIR`。Halcon 和 OpenCV 用上面的仓库内目录。

### 源码编码

源码按 UTF-8 保存，工程编译选项带 `/utf-8`。界面字符串用 `fromUtf8`。

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
