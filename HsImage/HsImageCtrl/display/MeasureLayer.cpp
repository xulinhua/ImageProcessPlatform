/**
 * @file MeasureLayer.cpp
 * @brief MeasureLayer实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MeasureLayer.h"
#include <HsCtrl/HsImageWindowTypes.h>
namespace HsCtrl {
static QString measureLabel(DrawTool hint, bool preview, const HsCommData::Graphic& g, const HsCommData::ImageCalib& cal)
{
    DrawTool kind = ToolBrowse;
    if (g.id.compare(0, 6, "m.dist") == 0) kind = ToolMeasureDist;
    else if (g.id.compare(0, 5, "m.ang") == 0) kind = ToolMeasureAngle;
    else if (g.id.compare(0, 5, "m.rad") == 0) kind = ToolMeasureRadius;
    else if (g.id.compare(0, 6, "m.rect") == 0) kind = ToolMeasureRect;
    else if (g.id.compare(0, 5, "m.ell") == 0) kind = ToolMeasureEllipse;
    else if (preview && (hint == ToolMeasureDist || hint == ToolMeasureAngle || hint == ToolMeasureRadius || hint == ToolMeasureRect || hint == ToolMeasureEllipse)) kind = hint;
    else return QString();
    if (kind == ToolMeasureDist && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjLine) {
        const HsCommData::LineP* ln = g.geom.lineAt(0);
        if (!ln) return QString();
        return QString::fromUtf8(HsCommData::formatLength(ln->b.x - ln->a.x, ln->b.y - ln->a.y, cal).c_str());
    }
    if (kind == ToolMeasureRadius && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjCircle) {
        const HsCommData::CircleP* c = g.geom.circleAt(0);
        if (!c) return QString();
        return QString::fromUtf8(HsCommData::formatRadius(c->r, cal).c_str());
    }
    if (kind == ToolMeasureAngle && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjContour) {
        const HsCommData::HsContour* ct = g.geom.contourAt(0);
        if (!ct || ct->ptCount() < 3) return QString();
        return QString::fromUtf8(HsCommData::formatAngle(ct->ptAt(0), ct->ptAt(1), ct->ptAt(2), cal).c_str());
    }
    if (kind == ToolMeasureRect && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjRect1) {
        const HsCommData::Rect1P* rc = g.geom.rect1At(0);
        if (!rc) return QString();
        const HsCommData::Box2 box = HsCommData::normalized(rc->r);
        return QString::fromUtf8(HsCommData::formatRect(HsCommData::width(box), HsCommData::height(box), cal).c_str());
    }
    if (kind == ToolMeasureEllipse && g.geom.isInitialized() && g.geom.kind() == HsCommData::ObjEllipse) {
        const HsCommData::EllipseP* el = g.geom.ellipseAt(0);
        if (!el) return QString();
        return QString::fromUtf8(HsCommData::formatEllipse(el->ra, el->rb, cal).c_str());
    }
    return QString();
}
MeasureLayer::MeasureLayer()
    : m_node(new LayerItem)
{
    m_node->setZValue(windowLayerZ(LayerMeasure));
}
void MeasureLayer::setViewScale(qreal scale)
{
    QHash<QString, GraphicItem*>::iterator it = m_items.begin();
    for (; it != m_items.end(); ++it) it.value()->setViewScale(scale);
}
void MeasureLayer::setVisible(bool visible) { m_node->setVisible(visible); }
GraphicItem* MeasureLayer::find(const QString& id) const { return m_items.value(id, 0); }
GraphicItem* MeasureLayer::upsert(const HsCommData::Graphic& g, qreal scale)
{
    const QString id = toQ(g.id);
    GraphicItem* item = m_items.value(id, 0);
    if (item) { item->setGraphic(g); return item; }
    item = new GraphicItem(g, m_node);
    item->setViewScale(scale);
    m_items.insert(id, item);
    return item;
}
void MeasureLayer::remove(const QString& id)
{
    GraphicItem* item = m_items.take(id);
    if (item) delete item;
}
void MeasureLayer::clear()
{
    const QList<QString> ids = m_items.keys();
    for (int i = 0; i < ids.size(); ++i) remove(ids[i]);
}
GraphicItem* MeasureLayer::hit(const QPointF& scenePos, qreal tol, const QString& preferId) const
{
    if (!m_node->isVisible()) return 0;
    GraphicItem* selected = m_items.value(preferId, 0);
    if (selected && selected->hitShape(scenePos, tol)) return selected;
    QList<GraphicItem*> list = m_items.values();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (list[i]->hitShape(scenePos, tol)) return list[i];
    }
    return 0;
}
QString MeasureLayer::labelText(DrawTool tool, bool preview, const HsCommData::Graphic& g, const HsCommData::ImageCalib& cal) const
{
    return measureLabel(tool, preview, g, cal);
}

// 图层节点
LayerItem* MeasureLayer::node() const
{
    return m_node;
}

// 图元表
QHash<QString, GraphicItem*>& MeasureLayer::items()
{
    return m_items;
}

// 图元表（只读）
const QHash<QString, GraphicItem*>& MeasureLayer::items() const
{
    return m_items;
}

// 是否含该 id
bool MeasureLayer::contains(const QString& id) const
{
    return m_items.contains(id);
}

}