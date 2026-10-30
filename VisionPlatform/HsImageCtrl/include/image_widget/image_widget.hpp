/**
 * @file image_widget.hpp
 * @brief 包入口。对外图像控件是 image_widget::ImageWidgetApi，
 *        画布是 HsCtrl::HsImageWindow，需要信号时用 imageWindow()。
 *
 *        HsImage 是 HsCtrl 的兼容别名：老代码里的 HsImage::HsImageWindow、
 *        WindowMode、DrawTool、视口和工具条枚举仍可用。
 *        图元类型是 HsCommData::Graphic。
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef IMAGE_WIDGET__IMAGE_WIDGET_HPP_
#define IMAGE_WIDGET__IMAGE_WIDGET_HPP_

#include <HsCtrl/HsImageCtrl.h>

namespace HsImage = HsCtrl;

#endif
