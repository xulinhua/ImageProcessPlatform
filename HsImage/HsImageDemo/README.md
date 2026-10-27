# HsImageDemo

`HsImageCtrl` 的交互演示程序。用于验证显示、ROI 绘制、叠层、换图策略、阈值刷 Overlay 等，**不是**正式产品界面。

## 职责

- 左侧工具条：浏览 / 圆 / 矩形 / 椭圆 / 线 / 十字 / 多边形 / 折线 / 文本 / 屏蔽涂抹等
- 中央：`ImageWidgetApi` 画布
- 右侧：画笔样式、层显隐、工具条停靠/皮肤、视口背景、文本面板、算法阈值试验区
- 文件：单张打开、文件夹顺序浏览、导出/快照（经窗口 API）
- 可选：HALCON 灰度阈值 → `HsRegion` → `setOverlay`（无 HALCON 时走本地扫描回退）

## 目录结构

```text
HsImageDemo/
  main.cpp
  MainWindow.h / .cpp     壳与全部 UI 逻辑
  HalconAlgo.h / .cpp     阈值绑定与刷 Overlay（可选 HALCON）
  HsImageDemo.vcxproj
```

## 依赖

| 依赖 | 说明 |
|------|------|
| HsImageCtrl / HsCommData | 链接同输出目录下的 `.lib` |
| Qt Widgets | 同 HsImageCtrl |
| HALCON（可选） | x64 且检测到头文件时定义 `HAS_HALCON` |

生成后事件会拷贝 Qt（及存在时的 HALCON）运行库到输出目录。

## 构建与运行

1. 生成 `HsImage.sln`（至少 HsCommData → HsImageCtrl → HsImageDemo）。
2. 运行：

```text
HsImage\bin\<Platform>\<Configuration>\HsImageDemo.exe
```

## 使用提示

- 窄字符串经 `QString::fromLocal8Bit`（源码 GBK），与平台一致。
- 「绘制会话」与直接 `setDrawTool` 并存：会话用于一次独占绘制；工具按钮可连续画 ROI。
- 阈值页用于压测 `setOverlay` 刷新路径，可开压力定时刷。

## 相关

- 控件能力说明：[../HsImageCtrl/README.md](../HsImageCtrl/README.md)
- 绘制会话 / 涂抹：[../docs/绘制与屏蔽方案.md](../docs/绘制与屏蔽方案.md)
