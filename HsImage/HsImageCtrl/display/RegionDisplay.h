/**
 * @file RegionDisplay.h
 * @brief RegionDisplay头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_REGIONDISPLAY_H
#define HSIMAGE_REGIONDISPLAY_H

#include <HsCommData/HsCommData.h>
#include <QColor>
#include <QPainterPath>
#include <QPixmap>

namespace HsCtrl {

/**
 * @class RegionDisplay
 * @brief HsCommData::HsRegion 的显示缓存。弦少时画 path，弦多时画 mask。
 */
class RegionDisplay {
public:
    HsCommData::HsRegion rle; ///< 与 Graphic.geom 里要显示的行程一致
    QPainterPath path;        ///< 填充轮廓
    QPainterPath outline;     ///< 边缘。外轮廓和孔洞，线框用
    QPixmap maskLod;          ///< 按视口缩小的贴图
    qreal maskScale;          ///< 生成 mask 时的 lod，1 表示不缩小

    RegionDisplay(); ///< 空显示缓存

    /** @brief 由 rle 生成填充 path */
    void rebuildPath();
    /** @brief 由区域边缘生成线框 path */
    void rebuildOutline();
    /**
     * @brief 按 lodScale 生成预乘 ARGB mask
     * @param lodScale 大于 1 会被夹回 1
     */
    void rebuildMask(const QColor& fill, qreal lodScale);
    /** @brief 无行程且无缓存 */
    bool isEmpty() const;

    /** @brief 弦多或面积大时改画 mask */
    static bool preferMask(const HsCommData::HsRegion& rle);
    /** @brief 由视口缩放得到 lod，夹到 [1/16, 1] */
    static qreal clampLodScale(qreal viewScale);
};

/** @brief 每条弦一个 1px 高矩形，拼成填充 path */
QPainterPath rleToPath(const HsCommData::HsRegion& rle);

}

#endif
