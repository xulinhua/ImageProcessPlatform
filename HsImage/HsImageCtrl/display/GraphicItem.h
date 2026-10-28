/**
 * @file GraphicItem.h
 * @brief GraphicItem头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_GRAPHICITEM_H
#define HSIMAGE_GRAPHICITEM_H

#include <HsCommData/HsCommData.h>
#include "RegionDisplay.h"

#include <QColor>
#include <QCursor>
#include <QGraphicsItem>
#include <QTransform>
#include <QVector>

namespace HsCtrl {

/**
 * @class HandleHit
 * @brief 手柄命中。index 与 handleScenePositions 下标一致。
 */
struct HandleHit {
    int index;        ///< 手柄下标，-1 表示未命中
    QPointF scenePos; ///< 命中的场景坐标
    HandleHit() : index(-1) {}
    /** @brief 是否命中有效手柄 */
    bool isValid() const;
};

/**
 * @class GraphicItem
 * @brief 一条 HsCommData::Graphic 的场景 Item。区域走行程缓存，不在 paint 里扫弦。
 */
class GraphicItem : public QGraphicsItem {
public:
    /** @brief 用协议里的 HsCommData::Graphic 建场景 Item */
    explicit GraphicItem(const HsCommData::Graphic& g, QGraphicsItem* parent = 0);

    /** @brief 替换协议数据并重建 HsCommData::HsRegion 缓存 */
    void setGraphic(const HsCommData::Graphic& g);
    /** @brief 当前协议数据副本 */
    HsCommData::Graphic graphic() const;
    /** @brief Drawing 选中（显示手柄） */
    void setSelectedDrawing(bool on);
    /** @brief 是否为当前选中 ROI */
    bool isSelectedDrawing() const;
    /** @brief 视口缩放，用于手柄粗细与 HsCommData::HsRegion LOD */
    void setViewScale(qreal s);
    /** @brief 是否为旋转手柄 */
    bool isRotationHandle(int index) const;
    /** @brief 该手柄在当前视口下的光标 */
    QCursor cursorForHandle(int index, const QTransform& sceneToView) const;

    /** @brief 含手柄垫的包围盒 */
    QRectF boundingRect() const override;
    /** @brief 加描边宽度后的命中形状 */
    QPainterPath shape() const override;
    /** @brief 按 kind 画几何，选中时画手柄 */
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    /**
     * @brief 视口坐标命中手柄
     * @param hotPx 命中热区，屏幕像素
     */
    HandleHit hitHandle(const QPointF& viewPos, const QTransform& sceneToView, qreal hotPx) const;
    /**
     * @brief 图像坐标命中轮廓或填充
     * @param imageTol 图像坐标容差
     */
    bool hitShape(const QPointF& scenePos, qreal imageTol) const;

    /** @brief 手柄在图像坐标系中的位置 */
    QVector<QPointF> handleScenePositions() const;

private:
    HsCommData::Graphic m_g;     ///< 协议数据
    RegionDisplay m_regionDisp;  ///< HsCommData::HsRegion 的 path/mask 缓存
    bool m_selected;             ///< Drawing 是否选中
    qreal m_viewScale;           ///< 视口缩放
    QRectF m_bounds;             ///< 含手柄垫的包围盒

    /** @brief 按几何与手柄重算包围盒 */
    void rebuildBounds();
    /** @brief 按 RLE 复杂度选择 path 或 mask */
    void ensureRegionDisplay();
    /** @brief mask 是否因缩放过期 */
    bool regionLodStale(qreal lod) const;
    /** @brief HsCommData::HsRegion 填充色 */
    QColor regionFillColor() const;
    /** @brief 把 mask 贴到 bounds 矩形 */
    void paintRegionMask(QPainter* painter, const QStyleOptionGraphicsItem* option) const;
    /** @brief 轮廓路径（命中与描边共用） */
    QPainterPath outlinePath() const;
    /** @brief 画选中手柄 */
    void paintHandles(QPainter* painter) const;
    /** @brief 从 painter 世界变换取当前缩放 */
    qreal viewScale(const QPainter* painter) const;
};

}

#endif
