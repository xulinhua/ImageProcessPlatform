/**
 * @file StackLayer.h
 * @brief StackLayer头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_STACKLAYER_H
#define HSIMAGE_STACKLAYER_H
#include "GraphicItem.h"
#include "LayerItem.h"
#include "QtGeom.h"
#include <QHash>
#include <memory>
namespace HsCtrl {
/** @class StackLayer @brief 叠层：结果几何、图元表与显隐 */
class StackLayer {
public:
    StackLayer();
    LayerItem* node() const;
    QHash<QString, GraphicItem*>& items();
    const QHash<QString, GraphicItem*>& items() const;
    std::shared_ptr<const HsCommData::FrameOverlay>& overlay();
    const std::shared_ptr<const HsCommData::FrameOverlay>& overlay() const;
    bool& wantVisible();
    HsCommData::GeomStyle& style();
    void setViewScale(qreal scale);
    void syncVisibility(const HsCommData::HsImage& frame);
    void rebuild(qreal scale);
    void clear();
    GraphicItem* hit(const QPointF& scenePos, qreal tol) const;
private:
    LayerItem* m_node;
    QHash<QString, GraphicItem*> m_items;
    std::shared_ptr<const HsCommData::FrameOverlay> m_overlay;
    bool m_visible;
    HsCommData::GeomStyle m_style;
};
}
#endif