/**
 * @file HsText.h
 * @brief HsText头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSTEXT_H
#define HSCOMMDATA_HSTEXT_H

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/Geom.h>

#include <string>

namespace HsCommData {

/**
 * @class HsText
 * @brief Overlay text. Anchor, content and font size stay together.
 */
struct HSCOMMDATA_EXPORT HsText {
    Point2d anchor;       ///< 锚点，图像坐标
    std::string content; ///< 正文

    HsText(); ///< 空正文
};

}

#endif
