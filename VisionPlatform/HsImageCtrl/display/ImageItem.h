/**
 * @file ImageItem.h
 * @brief ImageItem头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_IMAGEITEM_H
#define HSIMAGE_IMAGEITEM_H

#include "FrameConvert.h"

#include <QGraphicsItem>
#include <QImage>

namespace HsCtrl {

/**
 * @class ImageItem
 * @brief 底图。场景变换用 QTransform，paint 里 drawPixmap，不在上一帧上再采样。
 */
class ImageItem : public QGraphicsItem {
public:
    /** @brief 底图 Item，z=0 */
    explicit ImageItem(QGraphicsItem* parent = 0);

    /** @brief 换成新帧。只保存图像，绘制时再贴 */
    void setFrame(const HsCommData::HsImage& frame);
    /** @brief 直接换上已经备好的图 */
    void setImage(const QImage& image);
    /** @brief 清掉底图 */
    void clear();
    /** @brief 视口缩放，用来决定是否开 DeviceCoordinateCache */
    void setViewScale(qreal s);
    /** @brief 像素检验或接近 1:1 时关掉缓存，避免 HUD 取样错位 */
    void setPixelInspect(bool on);

    /** @brief 图像宽高对应的场景矩形 */
    QRectF boundingRect() const override;
    /** @brief 按需 drawImage。1:1 只贴可见像素 */
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    static const int kLodMaxEdge = 4096; ///< 长边超过此值才做缩小 LOD

private:
    /** @brief 按面积和检验开关选择缓存模式 */
    void updateCacheMode();

    QImage m_image;      ///< 底图像素。换图时不在主线程做成 QPixmap
    QSize m_imageSize;   ///< 底图像素尺寸
    qreal m_viewScale;   ///< 视口缩放
    bool m_pixelInspect; ///< 像素检验是否打开
};

}

#endif
