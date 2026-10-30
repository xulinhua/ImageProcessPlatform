# HsCommData

公共数据与几何协议库。**不依赖 Qt / OpenCV**，供算法、工具、显示控件与 VisionPlatform 共用同一套类型。

本库只描述「是什么」与「怎么存」；**不负责往屏幕上画**。绘制与交互在 [HsImageCtrl](../HsImageCtrl/README.md)。

对外总头：`include/HsCommData/HsCommData.h`。

---

## 1. 在整体中的位置

```text
┌─────────────────────────────────────────────────────────┐
│  VisionPlatform / 算法插件 / Demo                        │
│  读写 HsImage、HsObject、Graphic、FrameOverlay、HsTuple… │
└────────────────────────────▲────────────────────────────┘
                             │ 协议（本库）
┌────────────────────────────┴────────────────────────────┐
│  HsCommData.dll                                          │
│  几何 · 图像 · 区域 · 对象容器 · 叠层协议 · 元组 · 异常   │
└────────────────────────────▲────────────────────────────┘
                             │ 消费协议
┌────────────────────────────┴────────────────────────────┐
│  HsImageCtrl（Qt）                                       │
│  HsImageWindow / GraphicItem 把 Graphic 画到场景         │
└─────────────────────────────────────────────────────────┘
```

约定：

- **坐标系**：2D 图像坐标，原点左上，`x` 向右、`y` 向下，单位像素（可亚像素）。
- **帧对齐**：底图 `HsImage::frameId` 与 `FrameOverlay::frameId` / `Graphic::frameId` 一致时才画该帧 Stack。
- **拷贝语义**：`HsObject`、`HsTuple` 拷贝为**共享缓冲**，写入时拆开（COW）；`HsImage`、`HsRegion`、`HsContour` 等多为深拷。

---

## 2. 模块分层（自底向上）

```text
┌─ 系统 / 异常 ──────────────────────────────────────────┐
│  HsConst · HsSystem · HsError · HsException             │
├─ 基础几何 ─────────────────────────────────────────────┤
│  Geom.h（Point2 / Box2 / IRect）· GeomOps · Shape.h     │
│  Geom3 / Shape3（3D）                                    │
├─ 载体 ─────────────────────────────────────────────────┤
│  HsImage · HsRegion · HsContour · HsPoly · Mask         │
│  HsCloud / HsMesh / HsContour3                          │
├─ 容器 ─────────────────────────────────────────────────┤
│  HsObject（多类型对象袋）· HsTuple（控制参数元组）       │
├─ 显示协议（仍无 Qt）───────────────────────────────────┤
│  Color · Style · HsCross / HsArrow / HsText             │
│  Graphic · FrameOverlay                                 │
├─ 维数接口（自由函数）──────────────────────────────────┤
│  HsInterface2D · HsInterface3D                          │
└─────────────────────────────────────────────────────────┘
```

| 层 | 代表类型 | 作用 |
|----|----------|------|
| 基础几何 | `Point2`、`Box2`、`IRect`、`CircleP`… | 无分配的 POD / 轻量结构 |
| 载体 | `HsImage`、`HsRegion`、`HsContour` | 有缓冲或游程的「一块数据」 |
| 容器 | `HsObject`、`HsTuple` | 把多种载体/标量装进可变长度集合 |
| 显示协议 | `Graphic`、`Style`、`FrameOverlay` | 给控件看的「一条叠层 / 一帧结果」 |
| 标注附件 | `HsCross`、`HsArrow`、`HsText`、`MaskP` | 不走 `HsObject` 几何槽时的专用载荷 |

---

## 3. 数据结构关系总图

核心关系可以记成一句话：

> **一帧底图是 `HsImage`；一帧结果是 `FrameOverlay`（一袋 `Graphic`）；一条 `Graphic` 用 `kind` 决定载荷落在 `geom` 还是 `cross`/`text`/`mask`…；几何类载荷统一进 `HsObject`。**

```text
HsImage (底图)
  frameId ──────────────┐
                        │ 须一致才画 Stack
FrameOverlay            │
  frameId ?─────────────┘
  items[] ──? Graphic
                │
                ├─ role: RoleStack | RoleDrawing
                ├─ style: Style（描边/填充）
                ├─ kind ──┬─ KindObject / Keypoints / Skeleton
                │         │      └─? geom: HsObject
                │         │              ├─ CircleP / EllipseP / Rect1P / Rect2P / LineP
                │         │              ├─ HsContour / HsRegion / HsPoly
                │         │              └─ HsImage（少见，对象袋里也可装图）
                │         ├─ KindCross   ──? cross[]
                │         ├─ KindArrow   ──? arrow[]
                │         ├─ KindText / TextBadge ──? text[]
                │         ├─ KindPoint   ──? point[]
                │         └─ KindMask    ──? mask (MaskP)
                └─ boundsImage（updateBounds 重算）

HsTuple ── 独立：工具/算法控制参数（整数、浮点、字符串序列），不挂在 Graphic 上
```

### 3.1 基础几何 → 形状 → 载体

| 类型 | 文件 | 关系 |
|------|------|------|
| `Point2` / `Box2` / `IRect` | `Geom.h` | 一切 2D 坐标与框的基础 |
| `CircleP` / `EllipseP` / `Rect1P` / `Rect2P` / `LineP` | `Shape.h` | 由 `Point2`/`Box2` 组成；进入 `HsObject` 后对应 `ObjCircle`… |
| `HsContour` | `HsContour.h` | 点列 + 可选闭合/角度/响应；`ObjContour` |
| `HsRegion` | `HsRegion.h` | 行程编码（`Chord` 行游程）；可由 `IRect` 或点列 `genRegion`；`ObjRegion` |
| `HsPoly` | `HsPoly.h` | 多边形载体；`ObjPoly` |
| `HsImage` | `HsImage.h` | 像素缓冲 + `PixelKind`/`PixelEncoding`；也可作为 `ObjImage` |
| `MaskP` | `Mask.h` | 与底图同尺寸的 8bit 掩码；挂在 `Graphic::mask`，**不**进 `HsObject` |

3D（`Point3`、`Cloud3`、`HsMesh`、`HsContour3`、`Shape3`）与 2D 并列，当前显示控件主要消费 2D 协议；算法侧可先在本库存点云/网格。

### 3.2 HsObject：几何对象袋

`HsObject` 是「可变长度、可混放」的对象容器（思路接近 Halcon HObject，但类型集合是本方案自定义的）。

```text
HsObject
  └─ 共享 Body
       └─ ObjectUnion[]   每格一种 ObjectKind
            ├─ ObjCircle   → CircleP*
            ├─ ObjEllipse  → EllipseP*
            ├─ ObjRect1/2  → Rect1P* / Rect2P*
            ├─ ObjLine     → LineP*
            ├─ ObjContour  → HsContour*
            ├─ ObjRegion   → HsRegion*
            ├─ ObjPoly     → HsPoly*
            ├─ ObjImage    → HsImage*
            └─ ObjMixed    → 整袋混放时对外 kind
```

要点：

- 整袋统一一种时，`kind()` 返回该种；混放时为 `ObjMixed`，逐格用 `kindAt(i)`。
- **拷贝共享、写入拆开**；要独立副本用 `clone()`。
- 在显示协议里：`Graphic::geom` 就是一个 `HsObject`。
  - **Drawing（用户 ROI）**：通常只用第 0 个元素。
  - **Stack（结果）**：`KindObject` 可以一袋多个、种类不同；`geomSpan()` / `updateBounds()` 按 Stack/Drawing 规则扫个数。

### 3.3 Graphic：一条叠层或一条 ROI

`Graphic` **不负责绘制**，只描述「画什么、什么样式、属于哪一帧」。

| 字段 | 含义 |
|------|------|
| `id` | 唯一标识（控件侧增删改按 id） |
| `kind` | 载荷落在哪组字段（见下表） |
| `role` | `RoleStack` 结果只读叠层 / `RoleDrawing` 可拖 ROI |
| `style` | 描边、填充、线型、cosmetic 线宽等 |
| `frameId` | 所属帧，应与底图一致 |
| `boundsImage` | 图像坐标包围盒，改几何后调 `updateBounds()` |
| `geom` / `cross` / `arrow` / `text` / `point` / `mask` | 按 `kind` 选用 |

`GraphicKind` 与载荷对应：

| kind | 主载荷 | 备注 |
|------|--------|------|
| `KindObject` | `geom`（`HsObject`） | 圆/矩/线/轮廓/区域等；具体看 `geom.kind()` |
| `KindKeypoints` | `geom` 为轮廓 + `keypointVis` | 可见性与点对齐 |
| `KindSkeleton` | `geom` 为轮廓（成对为边）+ `edgeDashed` | |
| `KindCross` | `cross[]` | |
| `KindArrow` | `arrow[]` | |
| `KindText` / `KindTextBadge` | `text[]` | Badge 锚点仍在 text |
| `KindPoint` | `point[]` + `markerSize` | |
| `KindMask` | `mask` | 整幅 8bit，非行程 Region |

`Style` 提供缺省：`stackStyle()` / `drawingStyle()` / `highlightStyle()` / `annotationStyle()`。

### 3.4 FrameOverlay：一帧结果

```text
FrameOverlay
  frameId     // 必须对上当前 HsImage::frameId
  items[]     // 本帧全部 Stack 向 Graphic
```

- 控件 `setOverlay(shared_ptr)` 整帧替换 Stack。
- `dispCircle` 等是「在当前帧 overlay 上追加一条 Graphic」的快捷方式，内部仍构造 `FrameOverlay`。
- 客户不必每次手写 `frameId`：`disp*` 会取当前底图帧号。

### 3.5 HsTuple：控制参数（与 Graphic 正交）

`HsTuple` 存工具/算法的标量序列（纯 long / 纯 double / 纯 string，或 Mixed），**不是**显示图元。

- 访问：`I()`/`L()`/`D()`/`S()`/`C()`，支持负下标。
- 运算：比较、逐元素四则、广播长度等（实现见 `HsTuple.cpp`，声明见 `HsTupleOps.h`）。
- 与 `HsObject` 类似：共享缓冲 + 写时复制。

工具参数、序列结果用 Tuple；要画到窗口上的形状用 `Graphic`/`HsObject`。

### 3.6 区域 vs 掩码 vs 轮廓（易混）

| 类型 | 表示 | 典型用途 |
|------|------|----------|
| `HsRegion` | 行程（稀疏前景） | 阈值结果、多边形 ROI、画笔提交后的屏蔽区 |
| `MaskP` | 整图 8bit | 需要逐像素权值/灰度罩的 Stack 标注 |
| `HsContour` | 有序点列 | XLD、关键点骨架、未填充满的边界 |

多边形工具闭合 → 生成 `HsRegion` 放进 `Graphic.geom`（`ObjRegion`）。  
画笔会话确认 → 非 0 像素收成 `HsRegion`，同样作为 Drawing ROI，**不是** `KindMask`。

---

## 4. 目录结构

```text
HsCommData/
  include/HsCommData/     对外头（按模块拆分，总头 HsCommData.h）
  src/                    实现
  HsCommData.vcxproj
  HsCommData.vcxproj.filters   # 请用 UTF-8 BOM 保存
```

VS 筛选器大致对应：容器、2D/3D 图形、绘制、图像、掩膜、异常、系统。

---

## 5. 构建与使用

- 类型：`HsCommData.dll`；工程宏 `HSCOMMDATA_LIB`
- 工具集：v143，C++17；输出 `..\bin\<Platform>\<Configuration>\`
- 无 Qt 依赖，可被纯算法工程链接

```cpp
#include <HsCommData/HsCommData.h>
// 或叶头：
#include <HsCommData/HsImage.h>
#include <HsCommData/FrameOverlay.h>
```

链接 `HsCommData.lib`，运行目录放置 DLL。

### 最小协议示例（构造一条 Stack 圆）

```cpp
HsCommData::Graphic g;
g.kind = HsCommData::KindObject;
g.role = HsCommData::RoleStack;
g.style = HsCommData::Style::stackStyle();
g.geom.circleAt(0)->c = HsCommData::Point2(100, 80);
g.geom.circleAt(0)->r = 40;
g.updateBounds();

auto ov = HsCommData::FrameOverlay::create(frameId);
ov->items.push_back(g);
// 交给 HsImageWindow::setOverlay(ov);
```

---

## 6. 接入 VisionPlatform

```text
VisionPlatform\third_party\HsCommData\
  include\HsCommData\   # 头文件同步
  bin\<P>\<C>\          # HsCommData.dll
  lib\<P>\<C>\          # HsCommData.lib
```

平台经 `vision.props` 包含头路径并链接 `HsCommData.lib`；后事件从 `third_party\...\bin` 拷 DLL 到输出目录。

---

## 7. 注意

- 导出：`HsCommDataExport.h`；叶头只依赖导出头，保持无 Qt。
- 注释规范：仓库 `.cursor/rules/vision-comments.mdc`。
- 源码编码：UTF-8，工程编译选项带 `/utf-8`。
- 改协议后：同步平台 `third_party` 头文件 + 对应配置二进制，并检查 HsImageCtrl 是否仍匹配字段语义。
