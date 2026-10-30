# 三维工具分类与清单

对照国内通用平台（海康 VM3D、奥普特 Smart3、梅卡曼德 Mech-Vision）与 PCL 模块，按本平台 **2D 大类对称** 整理。  
算法实现放 `Plugin.Alg.Pcl.*` / `Plugin.Alg.Open3d.*`；缺算法 DLL 时运行提示未登记，与 2D 一致。

## 大类（ToolType）

| 大类 | 显示名 | 对应 2D | 市面对照 | 说明 |
|------|--------|---------|----------|------|
| `Acquire3D` | 三维采集 | 采集 | Smart3 3D 输入；VM 采集 | 文件/文件夹/相机点云或深度图入口 |
| `Prepare3D` | 点云预处理 | 预处理 | VM 点云/深度图处理；Smart3 3D 增强；Mech 滤波/降采样 | 滤波、裁剪、法向、合并、坐标变换、深度↔点云 |
| `Locate3D` | 三维定位 | 定位 | VM 3D 定位；Smart3 3D 定位检测；Mech 3D 匹配 | 模板匹配、ICP/NDT 配准、位姿精修（原 `Align3D`） |
| `Measure3D` | 三维测量 | — | VM 3D 测量；Smart3 3D 测量 | 拟合几何、高度/平面度/体积、点线面关系 |
| `Detect3D` | 三维检测 | 检测 | Smart3 三维缺陷/Blob；Mech 聚类 | 聚类分割、差分缺陷、区域生长 |
| `Calib3D` | 三维标定 | 标定 | VM 3D 标定；Smart3 手眼/三维标定 | 手眼、多相机外参 |

旧方案里的 `Align3D` 枚举值保留为 `Locate3D`（同一整数），`align3d.*` 工具名仍可解析到本大类。

## 工具清单

状态均为已挂工具树并可编译。运行时：PCL 为主实现；部分 Open3D 项会报「未实现」；相机/深度文件/手眼求解为明确失败占位，待接硬件或标定流程。

### 三维采集 `acquire3d.*`

| 工具名 | 显示名 | 状态 | 备注 |
|--------|--------|------|------|
| `acquire3d.load_cloud` | 加载点云 | 已有 | 单文件或文件夹 ASCII PLY/PCD |
| `acquire3d.camera` | 三维相机 | 已有 | 运行提示「未接三维相机」 |
| `acquire3d.depth_file` | 深度图文件 | 已有 | 深度图入口；加载未就绪时明确失败 |

### 点云预处理 `prepare3d.*`

| 工具名 | 显示名 | 状态 | PCL / Open3D | 市面对照 |
|--------|--------|------|--------------|----------|
| `prepare3d.downsample` | 降采样 | 已有 | VoxelGrid / VoxelDownSample | VM 点云降采样 |
| `prepare3d.crop` | 点云裁剪 | 已有 | CropBox / crop | VM 点云截取 |
| `prepare3d.outlier` | 离群点滤波 | 已有 | StatisticalOutlierRemoval | Mech 法向滤波/去离群 |
| `prepare3d.passthrough` | 直通滤波 | 已有 | PassThrough | 按轴范围裁剪 |
| `prepare3d.normals` | 法向估计 | 已有 | NormalEstimation | Mech 计算法向 |
| `prepare3d.merge` | 点云合并 | 已有 | += / append | VM 点云合并 |
| `prepare3d.transform` | 坐标变换 | 已有 | transformPointCloud | VM 坐标系转换 |
| `prepare3d.depth_to_cloud` | 深度图转点云 | 已有 | — | VM 转点云-深度图 |
| `prepare3d.cloud_to_depth` | 点云转深度图 | 已有 | — | VM 点云转深度图 |

### 三维定位 `locate3d.*`（兼容 `align3d.*`）

| 工具名 | 显示名 | 状态 | 备注 |
|--------|--------|------|------|
| `locate3d.icp` | ICP 配准 | 已有（现名 `align3d.icp`，保留兼容） | 点到点/点到面 |
| `locate3d.template` | 点云模板匹配 | 已有 | Mech 粗匹配+精匹配；Smart3 三维特征匹配 |
| `locate3d.ndt` | NDT 配准 | 已有 | PCL NDT |
| `locate3d.pose_refine` | 位姿精修 | 已有 | 匹配后微调 |

### 三维测量 `measure3d.*`

| 工具名 | 显示名 | 状态 | 备注 |
|--------|--------|------|------|
| `measure3d.fit_plane` | 拟合平面 | 已有 | VM 平面拟合 |
| `measure3d.fit_sphere` | 拟合球 | 已有 | SACMODEL_SPHERE |
| `measure3d.fit_cylinder` | 拟合圆柱 | 已有 | SACMODEL_CYLINDER |
| `measure3d.fit_line` | 拟合直线 | 已有 | 3D 直线 |
| `measure3d.fit_circle` | 拟合圆 | 已有 | 平面上圆 |
| `measure3d.height` | 点到面高度 | 已有 | 壳侧几何即可 |
| `measure3d.flatness` | 平面度 | 已有 | 相对拟合面峰谷/RMS |
| `measure3d.volume` | 体积 | 已有 | 相对基准面 |
| `measure3d.pp_dist` | 点点距离 | 已有 | VM 点点测量 |
| `measure3d.pl_dist` | 点线距离 | 已有 | |
| `measure3d.plane_plane` | 面面距离/夹角 | 已有 | VM 面面测量 |
| `measure3d.section` | 截面轮廓 | 已有 | 轮廓测量入口 |

### 三维检测 `detect3d.*`

| 工具名 | 显示名 | 状态 | 备注 |
|--------|--------|------|------|
| `detect3d.cluster` | 点云聚类 | 已有 | Euclidean Cluster；Mech 聚类 |
| `detect3d.diff` | 点云差分 | 已有 | Smart3 差分缺陷 |
| `detect3d.region_grow` | 区域生长 | 已有 | 平面/光滑分割 |

### 三维标定 `calib3d.*`

| 工具名 | 显示名 | 状态 | 备注 |
|--------|--------|------|------|
| `calib3d.hand_eye` | 手眼标定 | 已有 | |
| `calib3d.extrinsic` | 多相机外参 | 已有 | |

## 实现约定

1. **壳 / 算法拆分**：工具壳只做参数、端口、判定；算法在 PCL/Open3D 插件。高度、简单几何可只在壳内算。
2. **默认算法库**：`Prepare3D` / `Locate3D` / `Measure3D` / `Detect3D` 缺省 `AlgLibPcl`；可切 Open3D。
3. **不内置软算法兜底**：无对应算法 DLL 或 SDK 未编入时运行时报错，与 2D Halcon 一致。
4. **深度图 / 轮廓图**：首期以点云为主；转换类放在 `Prepare3D`，不单开大类（避免 VM 那种深度/轮廓/点云三套并行撑爆工具树）。

## 实现落地状态（PCL / Open3D）

对偶算法插件已按工具名分别实现，不再是 Downsample 克隆。

| 能力 | PCL | Open3D |
|------|-----|--------|
| downsample / crop / outlier / passthrough / normals | 已实现 | 已实现（passthrough 为轴过滤） |
| depth_to_cloud / cloud_to_depth | 图像缓冲实现 | 同左（不依赖 Open3D 深度 API） |
| icp / template / pose_refine | ICP | RegistrationICP |
| ndt | NDT | 运行时报 `Open3D 未实现` |
| fit_plane / fit_sphere | SAC | SegmentPlane / 球未实现 |
| fit_cylinder / fit_line / fit_circle | SAC | `Open3D 未实现` |
| section | 平面带宽截取 | 同左 |
| cluster | EuclideanCluster | DBSCAN |
| diff | KD 最近邻差分 | 同左 |
| region_grow | RegionGrowing | `Open3D 未实现` |

深度图转换：输入无有效 `HsImage` 时明确失败（`没有深度图` / `没有有效深度点`）。
