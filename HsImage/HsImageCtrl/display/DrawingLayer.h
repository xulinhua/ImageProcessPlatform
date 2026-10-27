/**
 * @file DrawingLayer.h
 * @brief DrawingLayer头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_DRAWINGLAYER_H
#define HSIMAGE_DRAWINGLAYER_H
#include "GraphicItem.h"
#include "LayerItem.h"
#include "QtGeom.h"
#include <QHash>
namespace HsCtrl {
/** @class DrawingLayer @brief 绘制层：设计态 ROI与显隐 */
class DrawingLayer {
public:
    DrawingLayer();
    LayerItem* node() const;
    QHash<QString, GraphicItem*>& items();
    const QHash<QString, GraphicItem*>& items() const;
    bool& wantVisible();
    HsCommData::GeomStyle& pen();
    void setViewScale(qreal scale);
    void setVisible(bool visible);
    GraphicItem* find(const QString& id) const;
    GraphicItem* upsert(const HsCommData::Graphic& g, qreal scale);
    void remove(const QString& id);
    void clear();
    GraphicItem* hit(const QPointF& scenePos, qreal tol, const QString& preferId) const;
private:
    LayerItem* m_node;
    QHash<QString, GraphicItem*> m_items;
    bool m_visible;
    HsCommData::GeomStyle m_pen;
};
}
#endif