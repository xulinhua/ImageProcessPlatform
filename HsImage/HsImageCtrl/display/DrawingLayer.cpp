/**
 * @file DrawingLayer.cpp
 * @brief DrawingLayer实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DrawingLayer.h"
#include <HsCtrl/HsImageWindowTypes.h>
namespace HsCtrl {
DrawingLayer::DrawingLayer()
    : m_node(new LayerItem)
    , m_visible(true)
    , m_pen(HsCommData::GeomStyle::drawingStyle())
{
    m_node->setZValue(windowLayerZ(LayerDrawing));
}
void DrawingLayer::setViewScale(qreal scale)
{
    QHash<QString, GraphicItem*>::iterator it = m_items.begin();
    for (; it != m_items.end(); ++it) {
        it.value()->setViewScale(scale);
    }
}
void DrawingLayer::setVisible(bool visible) { m_node->setVisible(visible); }
GraphicItem* DrawingLayer::find(const QString& id) const { return m_items.value(id, 0); }
GraphicItem* DrawingLayer::upsert(const HsCommData::Graphic& g, qreal scale)
{
    const QString id = toQ(g.id);
    GraphicItem* item = m_items.value(id, 0);
    if (item) {
        item->setGraphic(g);
        return item;
    }
    item = new GraphicItem(g, m_node);
    item->setViewScale(scale);
    m_items.insert(id, item);
    return item;
}
void DrawingLayer::remove(const QString& id)
{
    GraphicItem* item = m_items.take(id);
    if (item) {
        delete item;
    }
}
void DrawingLayer::clear()
{
    const QList<QString> ids = m_items.keys();
    for (int i = 0; i < ids.size(); ++i) {
        remove(ids[i]);
    }
}
GraphicItem* DrawingLayer::hit(const QPointF& scenePos, qreal tol, const QString& preferId) const
{
    if (!m_node->isVisible()) {
        return 0;
    }
    GraphicItem* selected = m_items.value(preferId, 0);
    if (selected && selected->hitShape(scenePos, tol)) {
        return selected;
    }
    QList<GraphicItem*> list = m_items.values();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list[i]->hitShape(scenePos, tol)) {
            return list[i];
        }
    }
    return 0;
}

// 图层节点
LayerItem* DrawingLayer::node() const
{
    return m_node;
}

// 图元表
QHash<QString, GraphicItem*>& DrawingLayer::items()
{
    return m_items;
}

// 图元表（只读）
const QHash<QString, GraphicItem*>& DrawingLayer::items() const
{
    return m_items;
}

// 可见性标志
bool& DrawingLayer::wantVisible()
{
    return m_visible;
}

// 画笔样式
HsCommData::GeomStyle& DrawingLayer::pen()
{
    return m_pen;
}

}