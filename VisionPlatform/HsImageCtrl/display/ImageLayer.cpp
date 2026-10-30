/**
 * @file ImageLayer.cpp
 * @brief ImageLayer实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ImageLayer.h"
#include <HsCtrl/HsImageWindowTypes.h>
namespace HsCtrl {
ImageLayer::ImageLayer()
    : m_item(new ImageItem)
{
    m_item->setZValue(windowLayerZ(LayerImage));
}
void ImageLayer::setFrame(const HsCommData::HsImage& frame)
{
    m_frame = frame;
    m_item->setFrame(frame);
}
void ImageLayer::clear()
{
    m_frame = HsCommData::HsImage();
    m_item->clear();
}
void ImageLayer::setVisible(bool visible) { m_item->setVisible(visible); }
bool ImageLayer::isVisible() const { return m_item->isVisible(); }
void ImageLayer::setViewScale(qreal scale) { m_item->setViewScale(scale); }
void ImageLayer::setPixelInspect(bool on) { m_item->setPixelInspect(on); }

// 图像项
ImageItem* ImageLayer::item() const
{
    return m_item;
}

// 底图帧
HsCommData::HsImage& ImageLayer::frame()
{
    return m_frame;
}

// 底图帧（只读）
const HsCommData::HsImage& ImageLayer::frame() const
{
    return m_frame;
}

}