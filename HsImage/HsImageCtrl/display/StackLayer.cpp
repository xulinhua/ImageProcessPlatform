/**
 * @file StackLayer.cpp
 * @brief StackLayer实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StackLayer.h"
#include <HsCtrl/HsImageWindowTypes.h>
#include <QSet>
#include <QList>
namespace HsCtrl {
StackLayer::StackLayer()
    : m_node(new LayerItem)
    , m_visible(true)
    , m_style(HsCommData::GeomStyle::stackStyle())
{
    m_node->setZValue(windowLayerZ(LayerStack));
}
void StackLayer::setViewScale(qreal scale)
{
    QHash<QString, GraphicItem*>::iterator it = m_items.begin();
    for (; it != m_items.end(); ++it) {
        it.value()->setViewScale(scale);
    }
}
void StackLayer::syncVisibility(const HsCommData::HsImage& frame)
{
    const bool match = m_overlay && !frame.isNull() && m_overlay->frameId == frame.frameId;
    m_node->setVisible(m_visible && match);
}
void StackLayer::rebuild(qreal scale)
{
    QSet<QString> seen;
    if (m_overlay) {
        for (size_t i = 0; i < m_overlay->items.size(); ++i) {
            HsCommData::Graphic g = m_overlay->items[i];
            if (g.role != HsCommData::RoleStack) {
                g.role = HsCommData::RoleStack;
            }
            if (g.geomStyle.stroke.isUnset()) {
                g.geomStyle = HsCommData::GeomStyle::stackStyle();
            }
            g.updateBounds();
            // 显示不按图形 id 合并。各工具都会写出 resultRoi.0，同名的都要画
            const QString key = QString::number(static_cast<qlonglong>(i));
            seen.insert(key);
            GraphicItem* item = m_items.value(key, 0);
            if (item) {
                item->setGraphic(g);
            } else {
                item = new GraphicItem(g, m_node);
                m_items.insert(key, item);
            }
            item->setZValue(static_cast<qreal>(i));
            item->setViewScale(scale);
        }
    }
    QList<QString> drop;
    QHash<QString, GraphicItem*>::iterator it = m_items.begin();
    for (; it != m_items.end(); ++it) {
        if (!seen.contains(it.key())) {
            drop.push_back(it.key());
        }
    }
    for (int i = 0; i < drop.size(); ++i) {
        GraphicItem* item = m_items.take(drop[i]);
        if (item) {
            delete item;
        }
    }
}
void StackLayer::clear()
{
    m_overlay.reset();
    rebuild(1.0);
}
GraphicItem* StackLayer::hit(const QPointF& scenePos, qreal tol) const
{
    if (!m_node->isVisible()) {
        return 0;
    }
    QHash<QString, GraphicItem*>::const_iterator it = m_items.constBegin();
    for (; it != m_items.constEnd(); ++it) {
        if (it.value()->hitShape(scenePos, tol)) {
            return it.value();
        }
    }
    return 0;
}

// 图层节点
LayerItem* StackLayer::node() const
{
    return m_node;
}

// 图元表
QHash<QString, GraphicItem*>& StackLayer::items()
{
    return m_items;
}

// 图元表（只读）
const QHash<QString, GraphicItem*>& StackLayer::items() const
{
    return m_items;
}

// 叠加场景
std::shared_ptr<const HsCommData::FrameOverlay>& StackLayer::overlay()
{
    return m_overlay;
}

// 叠加场景（只读）
const std::shared_ptr<const HsCommData::FrameOverlay>& StackLayer::overlay() const
{
    return m_overlay;
}

// 可见性标志
bool& StackLayer::wantVisible()
{
    return m_visible;
}

// 样式
HsCommData::GeomStyle& StackLayer::style()
{
    return m_style;
}

}