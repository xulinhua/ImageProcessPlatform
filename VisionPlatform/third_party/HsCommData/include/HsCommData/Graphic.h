/**
 * @file Graphic.h
 * @brief Graphic头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_GRAPHIC_H
#define HSCOMMDATA_GRAPHIC_H

#include <HsCommData/HsCommDataExport.h>

#include <HsCommData/Style.h>
#include <HsCommData/HsCross.h>
#include <HsCommData/HsArrow.h>
#include <HsCommData/HsText.h>
#include <HsCommData/HsObject.h>
#include <HsCommData/Mask.h>

#include <cstdint>
#include <string>
#include <vector>

namespace HsCommData {

/** @brief 绘制种类。几何只占 KindObject，具体圆或轮廓看 geom */
enum GraphicKind {
    KindUndefined = -1, ///< 未定义
    KindObject = 0, ///< 几何在 geom 里，种类可以不同
    KindPoint,      ///< 点
    KindCross,      ///< 十字
    KindText,       ///< 文本
    KindTextBadge,  ///< 分类徽章，锚点在 text
    KindMask,       ///< 灰度掩码
    KindKeypoints,  ///< 关键点，几何是轮廓
    KindSkeleton,   ///< 骨架，几何是轮廓，成对为边
    KindArrow,       ///< 箭头，几何在 arrow
    KindMax ///< 个数，不是合法值
};

/** @brief Stack 是结果叠加，只读；Drawing 是可拖的 ROI */
enum GraphicRole {
    RoleStack = 0, ///< 结果叠加
    RoleDrawing    ///< 交互 ROI
};

/**
 * @class Graphic
 * @brief 一条叠加或 ROI。本类不负责往屏幕上画
 */
class HSCOMMDATA_EXPORT Graphic {
public:
    std::string id;          ///< 唯一 id
    std::string layerId;     ///< 图层 id（预留）
    GraphicKind kind;        ///< 绘制种类
    GraphicRole role;        ///< Stack 或 Drawing
    GeomStyle   geomStyle;   ///< 几何、十字、箭头、点
    TextStyle   textStyle;   ///< 文字
    int64_t     frameId;     ///< 所属帧，须与底图一致
    Box2        boundsImage; ///< 图像坐标包围盒
    bool        visible;     ///< 是否参与绘制
    HsObject    geom;        ///< KindObject / 关键点 / 骨架。Stack 可放多个，种类可以不同；Drawing 只用第 0 个
    std::vector<HsCross> cross; ///< KindCross。Stack 可放多个；Drawing 只用第 0 个
    std::vector<HsArrow> arrow; ///< KindArrow。Stack 可放多个；Drawing 只用第 0 个
    std::vector<HsText>  text;  ///< KindText / KindTextBadge。Stack 可放多个；Drawing 只用第 0 个
    std::vector<Point2d>  point; ///< KindPoint 位置。Stack 可放多个；Drawing 只用第 0 个
    double markerSize;          ///< KindPoint 半宽（像素），整批共用
    MaskP     mask;      ///< KindMask
    std::vector<int> keypointVis;          ///< KindKeypoints：与 pts 对齐，1 遮挡空心，2 可见实心
    std::vector<unsigned char> edgeDashed; ///< KindSkeleton：逐边 1 为虚线

    Graphic(); ///< 默认圆、Stack、可见

    /**
     * @brief 包围盒和绘制要扫过的个数
     * Drawing 只用第 0 个，容器空则为 0。KindObject、关键点、骨架在 Stack 上为 geom 的个数。点、十字、箭头、文字在 Stack 上为各自容器的个数。掩膜为 1
     */
    int geomSpan() const;

    /** @brief 按 kind 重算 boundsImage。Stack 取每个元素自己种类的并集 */
    void updateBounds();
};

}

#endif
