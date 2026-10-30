# HsImageCtrl

基于 Qt Widgets 的图像显示与交互库。协议全部来自 [HsCommData](../HsCommData/README.md)；**对外只谈协议类型，不暴露内部 `QGraphicsItem`**。

总头：`include/HsCtrl/HsImageCtrl.h`（协议总头 + 窗口 + `ImageWidgetApi`）。

设计文档：[../docs/绘制与屏蔽方案.md](../docs/绘制与屏蔽方案.md)。

---

## 1. 架构总览

```text
业务 / VisionPlatform / Demo
        │
        │  嵌入布局优先
        ▼
┌─────────────────── image_widget::ImageWidgetApi ───────────────────┐
│  工具条 · 信息栏 · 常用转发 API                                      │
│  widget() → 外壳 QWidget                                            │
│  imageWindow() → 需要信号/高级 API 时取出内部窗口                   │
└────────────────────────────┬───────────────────────────────────────┘
                             │ 持有
                             ▼
┌─────────────────── HsCtrl::HsImageWindow : QGraphicsView ──────────┐
│  对外：setImage / disp* / setOverlay / setDrawings / 工具与会话     │
│  对内：Pimpl（Private）驱动场景与交互状态机                         │
└────────────────────────────┬───────────────────────────────────────┘
                             │ 场景坐标 = 图像像素 1:1
                             ▼
┌─────────────────── QGraphicsScene ─────────────────────────────────┐
│  ImageItem          z≈0     底图（HsImage → 显示缓冲）              │
│  stackLayer         z≈10    Stack：FrameOverlay → GraphicItem*     │
│  drawingLayer       z≈20    Drawing：用户 ROI GraphicItem*         │
│  （前景另画）HUD / 像素检验 / drawInfo / 画笔预览 — 不进场景 Item   │
└────────────────────────────────────────────────────────────────────┘
                             ▲
                             │ 只读协议
                      HsCommData::Graphic / FrameOverlay / Style …
```

分层原则：

| 层 | 职责 | 不负责 |
|----|------|--------|
| `ImageWidgetApi` | 产品嵌入壳、常用方法转发 | 不另搞一套协议 |
| `HsImageWindow` | 视口、交互、叠层增删、信号 | 不实现算法 |
| `GraphicItem` | 把一条 `Graphic` 画成可命中图元 | 不改协议字段语义 |
| HsCommData | 类型与存储 | 不依赖本 DLL |

---

## 2. 与 HsCommData 的数据流

```text
                    setImage / setCompressedImage / setImage(Mat)
                                      │
                                      ▼
                               HsImage（底图）
                               frameId, WxH, pixels
                                      │
          ┌───────────────────────────┼───────────────────────────┐
          │                           │                           │
          ▼                           ▼                           ▼
   ImageItem 贴图              frameId 对齐检查              ImageRefreshPolicy
                                                      （换图是否清 Stack/ROI/字）

   setOverlay(FrameOverlay)          dispCircle / dispRegion / …
          │                                    │
          │ 整帧替换                             │ 追加一条 Graphic 到当前帧
          └────────────────┬───────────────────┘
                           ▼
                    Private::overlay
                    rebuildStack()
                           │
                           ▼
              stackLayer 上的 GraphicItem 字典（按 Graphic.id）

   setDrawings / 鼠标绘制 / 绘制会话
          │
          ▼
   drawingItems 字典 → drawingLayer
          │
          ▼
   roiCommitted / roiSelected / drawSessionFinished
   （信号载荷仍是 HsCommData::Graphic）
```

### 2.1 协议字段在控件里怎么用

| HsCommData | 控件行为 |
|------------|----------|
| `HsImage` | 底图唯一数据源；采样灰度/RGB、像素检验都读它 |
| `FrameOverlay` | Stack 的权威快照；`rebuildStack` 与场景同步 |
| `Graphic.role` | Stack 进 `stackLayer`；Drawing 进 `drawingLayer`（写入时也会强制纠正） |
| `Graphic.kind` + 载荷 | `GraphicItem` 按 kind 画几何/十字/字/掩码等 |
| `Graphic.geom`（`HsObject`） | ROI 与多数结果形状；Drawing 只用第 0 元 |
| `Graphic.style` | 笔色线宽；缺省用 `stackStyle` / `drawingStyle` |
| `Graphic.frameId` | 与底图不一致时 Stack 层可隐藏（帧未对齐） |
| `HsRegion` | 多边形闭合、画笔提交、阈值类结果的常用载体 |
| `MaskP` | `KindMask` 整幅罩；与画笔生成的 Region **不是**同一路径 |

### 2.2 Stack vs Drawing（务必分清）

```text
Stack（RoleStack）              Drawing（RoleDrawing）
─────────────────              ──────────────────────
算法/业务结果                  用户交互 ROI / 文本
通常只读、整帧替换             可拖手柄、可多选流程编辑
FrameOverlay / disp*           setDrawings / 鼠标工具 / 会话
z = stackLayer                 z = drawingLayer（更高）
运行态仍可显示                 运行态强制隐藏
```

同一条逻辑形状两边都能表达（例如都是圆），差别在 **角色与生命周期**：结果跟帧走；ROI 跟编辑会话走。

### 2.3 工具枚举 → 协议类型

`DrawTool`（控件）落到 `Graphic` 时大致如下：

| DrawTool | 结果 Graphic | 几何落点 |
|----------|---------------|----------|
| `ToolCircle` | `KindObject` | `ObjCircle` |
| `ToolRect1` / `ToolRect2` / `ToolEllipse` / `ToolLine` | `KindObject` | 对应 Shape |
| `ToolContour` | `KindObject` | `ObjContour`（折线） |
| `ToolPolygon` | `KindObject` | `ObjRegion`（闭合填成行程） |
| `ToolCross` | `KindCross` | `cross[0]` |
| `ToolText` | `KindText` | `text[0]` |
| `ToolBrush` | `KindObject` | 确认后 `ObjRegion`（仅会话内可用） |
| `ToolBrowse` | — | 不落笔 |

`setDrawTool`：换工具，可连续画（Drawing Object 风格）。  
`beginDrawSession(tool)`：独占一次，结束发 `drawSessionFinished`（接近 Halcon `draw_*`）。

---

## 3. 窗口内部结构（Pimpl）

`HsImageWindow::Private` 是状态中枢，逻辑分组如下：

```text
Private
├─ 场景与图元
│   scene · imageItem · stackLayer · drawingLayer
│   stackItems / drawingItems   // id → GraphicItem*
├─ 协议缓存
│   frame: HsImage
│   overlay: shared_ptr<const FrameOverlay>
├─ 视口
│   Viewport vp   // part（可见矩形）+ imageSize；缩放=视口变换，不改场景坐标
├─ 交互
│   mode · tool · state（Browse / Pan / Draw / EditHandle）
│   activeId · activeHandle · rubberActive · polyPts
│   sessionActive / sessionDraft / brush* …
├─ 表现
│   hudVisible · pixelInspect · penStyle · overlayStyle
│   imageDrawInfos / windowDrawInfos（设备坐标文字，非 Graphic）
└─ 壳
    overlayBar · ImageRefreshPolicy · ViewportBackground …
```

### 3.1 坐标与缩放

- **场景坐标 = 图像像素**，圆半径、ROI 顶点都以图像像素计。
- 缩放/平移只改 `QGraphicsView` 变换与 `part`，**不**把矢量字号塞进场景（`drawInfo` / HUD 在设备坐标画，避免糊或看不见）。
- `widgetToImage` / `imageToWidget` / `mapWidgetToImagePixel` 供外部取样与落点。

### 3.2 换底图策略

`ImageRefreshPolicy`（默认全 `false` = 全保留）：

| 标志 | true 时 |
|------|---------|
| `refreshStack` | 清空 Stack / overlay |
| `refreshDrawing` | 清几何 ROI（不含 `KindText`） |
| `refreshText` | 清 `drawInfo` 与 Drawing 文字 |

与「清图但留 ROI」的常见产品行为对齐；需要跟帧清空时由业务显式打开。

### 3.3 设计态 / 运行态

| `WindowMode` | 行为 |
|--------------|------|
| `WindowDesign` | 可切换工具、画 ROI、拖手柄 |
| `WindowRuntime` | 强制浏览；Drawing 层隐藏；仍可看底图与 Stack |

---

## 4. 目录结构

```text
HsImageCtrl/
  include/
    HsCtrl/
      HsImageCtrl.h           # 总头
      HsImageWindow.h         # 窗口 API + 信号
      HsImageWindowTypes.h    # 模式/工具/换图策略等枚举
      HsImgCtrlExport.h
    image_widget/
      image_widget_api.hpp    # 嵌入壳
  display/                    # HsImageWindow.cpp、GraphicItem、图层…
  canvas/
  protocol/                   # QImage ? HsImage 等转换
  HsImageCtrl.vcxproj
```

---

## 5. 对外 API 地图（按用途）

### 5.1 嵌入

```cpp
#include <HsCtrl/HsImageCtrl.h>

image_widget::ImageWidgetApi api(parent);
layout->addWidget(api.widget());

api.setImage(qimage);                    // 常用
HsCtrl::HsImageWindow* w = api.imageWindow();
connect(w, &HsCtrl::HsImageWindow::roiCommitted, …);
```

### 5.2 底图

- `setImage` / `setImageRaw` / `setImage(cv::Mat)` / `setCompressedImage`
- `clearImage`、`hasImage`、`currentImageSize`、`currentFrameId`
- `setImageRefreshPolicy`

### 5.3 Stack（结果）

- 快捷：`dispCircle` / `dispEllipse` / `dispRect1` / `dispRect2` / `dispLine` / `dispCross` / `dispArrow` / `dispRegion` / `dispContour`…
- 整帧：`setOverlay(shared_ptr<FrameOverlay>)`、`setOverlays(vector)`、`clearWindow`
- 显隐：`setStackVisible` / `setOverlayVisible`（别名）

### 5.4 Drawing（ROI）

- 批量：`setDrawings` / `drawings` / `clearDrawings` / `setDrawing`
- 样式：`setDrawingStyle` / `setPenColor` / `setPenWidth`
- 工具：`setDrawTool`、`beginDrawSession` / `commitDrawSession` / `cancelDrawSession`
- 信号：`roiCommitted`、`roiSelected`、`drawSessionFinished`、`textAnchorPicked`

### 5.5 叠加文字（非 Graphic）

- `drawInfo` / `addDrawInfo` / `clearDrawInfo`（图像坐标或窗口坐标）
- 与 Drawing 的 `KindText` 不同：前者是视口前景提示，后者是可进 ROI 列表的图元。

### 5.6 视口与壳

- `fitToWindow` / `zoomActualSize` / `zoomIn` / `zoomOut` / `part` / `setPart`
- 浮动工具条、视口背景、HUD、像素检验、截图 `saveSnapshot` / `saveCurrentFrame`

---

## 6. 交互状态机（简图）

```text
        setDrawTool / beginDrawSession
                    │
                    ▼
              StateBrowse ?──── Esc / 取消 / 会话结束
                 │    ▲
        中键拖    │    │ 松开
                 ▼    │
              StatePan
                 │
        落笔画 ROI（非折线）
                 ▼
              StateDraw ──松开──?（非会话）commitGraphic
                 │                 （会话）sessionDraft，可再拖手柄
                 │
        点中手柄/本体
                 ▼
           StateEditHandle ──松开──? roiCommitted（非会话）

折线/多边形：多点 → 右键或 Enter 结束；Backspace 退点
画笔：仅会话；左涂右擦；确认后 Region 进 Drawing
```

更细的产品约束见 `docs/绘制与屏蔽方案.md`。

---

## 7. 依赖与构建

| 依赖 | 说明 |
|------|------|
| HsCommData | 工程引用；链接同输出目录 `HsCommData.lib` |
| Qt 5.15.2 Widgets | 环境变量 `QTDIR` 指向 msvc2019_64（Win32 为 msvc2019） |
| OpenCV（可选） | 有头文件时启用 `setImage(cv::Mat)` |

- 工程宏：`HSCTRL_LIB`
- 输出：`..\bin\<Platform>\<Configuration>\HsImageCtrl.dll|.lib`
- MOC：`..\tools\run_moc.bat` 处理 `HsImageWindow.h`
- 源码编码：UTF-8（与平台一致），编译选项 `/utf-8`

---

## 8. 接入 VisionPlatform

```text
VisionPlatform\third_party\HsImageCtrl\
  include\          # HsCtrl + image_widget
  bin\<P>\<C>\      # HsImageCtrl.dll
  lib\<P>\<C>\      # HsImageCtrl.lib
```

同时保证同配置的 **HsCommData** 头与 DLL 已同步。平台 UI 链接 `HsImageCtrl.lib`，运行时两个 DLL 都要在 exe 旁（或 PATH）。

推荐嵌入路径：布局只拿 `ImageWidgetApi::widget()`；需要 ROI 信号时再 `imageWindow()`。

---

## 9. 扩展时注意

1. **新形状**：先在 HsCommData 增加 Shape/`ObjectKind`/`Graphic` 载荷，再在 `GraphicItem` 画与命中，最后才加 `DrawTool`。
2. **不要**让业务直接操作 `QGraphicsItem`；进出边界保持 `Graphic` / `FrameOverlay`。
3. Stack 跟 `frameId`；换图策略默认保留叠层，避免误清现场 ROI。
4. Drawing 的 `KindText` 与 `drawInfo` 文字是两条线，换图 `refreshText` / `refreshDrawing` 分开控制。
