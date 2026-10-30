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
    /** @brief 建图层节点，Z 序取 LayerStack，样式取 stackStyle */
    StackLayer();
    /************************ 访问 ************************/
    /** @brief 图层根节点 */
    LayerItem* node() const;
    /** @brief 序号 → 图元表，键为 overlay 中的下标 */
    QHash<QString, GraphicItem*>& items();
    /** @brief 序号 → 图元表（只读） */
    const QHash<QString, GraphicItem*>& items() const;
    /** @brief 当前帧叠加快照 */
    std::shared_ptr<const HsCommData::FrameOverlay>& overlay();
    /** @brief 当前帧叠加快照（只读） */
    const std::shared_ptr<const HsCommData::FrameOverlay>& overlay() const;
    /** @brief 用户期望的显隐开关 */
    bool& wantVisible();
    /** @brief Stack 样式 */
    HsCommData::GeomStyle& style();
    /************************ 操作 ************************/
    /** @brief 把视图缩放同步给全部图元 */
    void setViewScale(qreal scale);
    /** @brief 开关打开且 overlay 帧号与底图一致时才显示 */
    void syncVisibility(const HsCommData::HsImage& frame);
    /**
     * @brief 按 overlay 重建图元：按下标一一对应复用，不按 id 合并；
     *        role 强制为 Stack，未设描边的用 stackStyle，多出的旧图元删除
     */
    void rebuild(qreal scale);
    /** @brief 丢掉 overlay 并删除全部图元 */
    void clear();
    /** @brief 命中测试：图层隐藏时返回空，否则返回 QHash 遍历中第一个命中的图元 */
    GraphicItem* hit(const QPointF& scenePos, qreal tol) const;
private:
    LayerItem* m_node;                                         ///< 图层根节点，归场景所有
    QHash<QString, GraphicItem*> m_items;                      ///< 下标 → 图元
    std::shared_ptr<const HsCommData::FrameOverlay> m_overlay; ///< 当前帧叠加快照
    bool m_visible;                                            ///< 用户期望显隐
    HsCommData::GeomStyle m_style;                             ///< Stack 样式
};
}
#endif