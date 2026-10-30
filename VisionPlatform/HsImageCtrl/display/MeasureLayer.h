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
    /** @brief 建图层节点，Z 序取 LayerMeasure */
    MeasureLayer();
    /************************ 接口 ************************/
    /** @brief 图层根节点 */
    LayerItem* node() const;
    /** @brief id → 测量图元表 */
    QHash<QString, GraphicItem*>& items();
    /** @brief id → 测量图元表（只读） */
    const QHash<QString, GraphicItem*>& items() const;
    /** @brief 把视图缩放同步给全部图元 */
    void setViewScale(qreal scale);
    /** @brief 显隐图层节点 */
    void setVisible(bool visible);
    /** @brief 按 id 查图元，没有返回空 */
    GraphicItem* find(const QString& id) const;
    /** @brief 是否有该 id 的测量 */
    bool contains(const QString& id) const;
    /** @brief 已有同 id 则更新几何，否则新建挂到节点下 */
    GraphicItem* upsert(const HsCommData::Graphic& g, qreal scale);
    /** @brief 删除并释放一个图元 */
    void remove(const QString& id);
    /** @brief 删除全部测量 */
    void clear();
    /** @brief 命中测试：图层隐藏时返回空；先试 preferId，再按 QHash 遍历倒序试其余图元 */
    GraphicItem* hit(const QPointF& scenePos, qreal tol, const QString& preferId) const;
    /**
     * @brief 测量读数文字。按 id 前缀（m.dist / m.ang / m.rad / m.rect / m.ell）判类型，
     *        预览时 id 无前缀则用 tool；类型或几何不符返回空串
     */
    QString labelText(DrawTool tool, bool preview, const HsCommData::Graphic& g, const HsCommData::ImageCalib& cal) const;
private:
    LayerItem* m_node;                    ///< 图层根节点，归场景所有
    QHash<QString, GraphicItem*> m_items; ///< id → 测量图元
};
}
#endif