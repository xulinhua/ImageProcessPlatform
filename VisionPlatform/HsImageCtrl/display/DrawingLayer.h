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
    /** @brief 建图层节点，Z 序取 LayerDrawing，画笔取 drawingStyle */
    DrawingLayer();
    /************************ 访问 ************************/
    /** @brief 图层根节点，图元都挂在它下面 */
    LayerItem* node() const;
    /** @brief id → 图元表 */
    QHash<QString, GraphicItem*>& items();
    /** @brief id → 图元表（只读） */
    const QHash<QString, GraphicItem*>& items() const;
    /** @brief 用户期望的显隐开关（运行态下节点可能仍被隐藏） */
    bool& wantVisible();
    /** @brief 新绘 ROI 的画笔样式 */
    HsCommData::GeomStyle& pen();
    /************************ 操作 ************************/
    /** @brief 把视图缩放同步给全部图元，用于屏幕线宽与手柄尺寸 */
    void setViewScale(qreal scale);
    /** @brief 显隐图层节点 */
    void setVisible(bool visible);
    /** @brief 按 id 查图元，没有返回空 */
    GraphicItem* find(const QString& id) const;
    /** @brief 已有同 id 则更新几何，否则新建挂到节点下 */
    GraphicItem* upsert(const HsCommData::Graphic& g, qreal scale);
    /** @brief 删除并释放一个图元 */
    void remove(const QString& id);
    /** @brief 删除全部图元 */
    void clear();
    /** @brief 命中测试：图层隐藏时返回空；先试 preferId，再按 QHash 遍历倒序试其余图元 */
    GraphicItem* hit(const QPointF& scenePos, qreal tol, const QString& preferId) const;
private:
    LayerItem* m_node;                    ///< 图层根节点，归场景所有
    QHash<QString, GraphicItem*> m_items; ///< id → 图元
    bool m_visible;                       ///< 用户期望显隐
    HsCommData::GeomStyle m_pen;          ///< 新绘 ROI 画笔
};
}
#endif