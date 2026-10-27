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
    ImageLayer();
    ImageItem* item() const;
    HsCommData::HsImage& frame();
    const HsCommData::HsImage& frame() const;
    void setFrame(const HsCommData::HsImage& frame);
    void clear();
    void setVisible(bool visible);
    bool isVisible() const;
    void setViewScale(qreal scale);
    void setPixelInspect(bool on);
private:
    ImageItem* m_item;
    HsCommData::HsImage m_frame;
};
}
#endif