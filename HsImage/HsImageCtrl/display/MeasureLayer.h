/**
 * @file MeasureLayer.h
 * @brief MeasureLayer头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_MEASURELAYER_H
#define HSIMAGE_MEASURELAYER_H
#include "GraphicItem.h"
#include "LayerItem.h"
#include "QtGeom.h"
#include <HsCtrl/HsImageWindowTypes.h>
#include <QHash>
namespace HsCtrl {
/** @class MeasureLayer @brief 测量层：临时测量图形与显隐 */
class MeasureLayer {
public:
    MeasureLayer();
    LayerItem* node() const;
    QHash<QString, GraphicItem*>& items();
    const QHash<QString, GraphicItem*>& items() const;
    void setViewScale(qreal scale);
    void setVisible(bool visible);
    GraphicItem* find(const QString& id) const;
    bool contains(const QString& id) const;
    GraphicItem* upsert(const HsCommData::Graphic& g, qreal scale);
    void remove(const QString& id);
    void clear();
    GraphicItem* hit(const QPointF& scenePos, qreal tol, const QString& preferId) const;
    QString labelText(DrawTool tool, bool preview, const HsCommData::Graphic& g, const HsCommData::ImageCalib& cal) const;
private:
    LayerItem* m_node;
    QHash<QString, GraphicItem*> m_items;
};
}
#endif