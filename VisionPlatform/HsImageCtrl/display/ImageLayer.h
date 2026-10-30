/**
 * @file ImageLayer.h
 * @brief ImageLayer头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_IMAGELAYER_H
#define HSIMAGE_IMAGELAYER_H
#include "ImageItem.h"
#include <HsCtrl/HsImageWindowTypes.h>
namespace HsCtrl {
/** @class ImageLayer @brief 底图图层：帧与底图节点 */
class ImageLayer {
public:
    /** @brief 建底图节点，Z 序取 LayerImage */
    ImageLayer();
    /** @brief 底图图元 */
    ImageItem* item() const;
    /** @brief 当前帧（取样与 HUD 用） */
    HsCommData::HsImage& frame();
    /** @brief 当前帧（只读） */
    const HsCommData::HsImage& frame() const;
    /** @brief 换帧，并交给底图图元显示 */
    void setFrame(const HsCommData::HsImage& frame);
    /** @brief 清空帧与底图 */
    void clear();
    /** @brief 底图显隐 */
    void setVisible(bool visible);
    /** @brief 底图是否可见 */
    bool isVisible() const;
    /** @brief 同步视图缩放给底图图元 */
    void setViewScale(qreal scale);
    /** @brief 像素检验开关，透传给底图图元 */
    void setPixelInspect(bool on);
private:
    ImageItem* m_item;           ///< 底图图元，归场景所有
    HsCommData::HsImage m_frame; ///< 当前帧
};
}
#endif