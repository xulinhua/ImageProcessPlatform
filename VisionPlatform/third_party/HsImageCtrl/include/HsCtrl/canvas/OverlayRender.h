/**
 * @file OverlayRender.h
 * @brief OverlayRender头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_CANVAS_OVERLAYRENDER_H
#define HSIMAGE_CANVAS_OVERLAYRENDER_H

#include <HsCtrl/HsImgCtrlExport.h>
#include <HsCommData/HsCommData.h>

#include <QTransform>
#include <vector>

class QPainter;

namespace HsCtrl {

/**
 * @class OverlayRender
 * @brief 在已设好的图像坐标系里画一条或一批 HsCommData::Graphic。
 *
 * HsCommData::KindRegion 的 mask LOD 仍由 GraphicItem 负责，这里只画其余 kind。
 */
class HSCTRL_EXPORT OverlayRender {
public:
    /**
     * @brief 在当前变换下画一条 HsCommData::Graphic
     * @param selected true 时用高亮笔
     */
    static void paintScene(QPainter* painter, const HsCommData::Graphic& g, bool selected);
    /**
     * @brief 套上 image→device 后画一批 HsCommData::Graphic
     */
    static void paintList(QPainter* painter, const std::vector<HsCommData::Graphic>& items,
                          const QTransform& imageToDevice);
};

}

#endif
