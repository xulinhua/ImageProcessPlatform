/**
 * @file IconAssets.h
 * @brief IconAssets头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ICONASSETS_H
#define VISION_ICONASSETS_H
#include "VisionUiExport.h"

#include <QColor>
#include <QIcon>
#include <QString>

namespace Vision {

/**
 * @brief 从 exe/icons/ 加载矢量/位图图标
 * @param relativePath 相对路径，如 tools/camera.svg、ui/save.svg
 * @param px 渲染边长（像素）
 * @return 失败时返回空 QIcon
 */
VISION_UI_API QIcon loadVisionIcon(const QString& relativePath, int px = 24);

/**
 * @brief 在指定底色圆角矩形上叠绘图标（工具库分类用）
 * @param icon 前景图标
 * @param bg 背景色
 * @param px 输出边长
 */
VISION_UI_API QIcon badgeVisionIcon(const QIcon& icon, const QColor& bg, int px = 24);

}

#endif
