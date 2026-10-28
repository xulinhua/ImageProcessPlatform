/**
 * @file RawArchive.h
 * @brief 一拍结束先把原图落到本机目录
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RAW_ARCHIVE_H
#define VISION_RAW_ARCHIVE_H
#include "VisionRunExport.h"

#include <HsCommData/HsImage.h>
#include <string>

namespace Vision {

/**
 * @class RawArchive
 * @brief 路径 {rawDir}/{stationId}/{yyyyMMdd}/{frameSeq}.png
 * @note 第一版不把这个文件提供给远端打开
 */
class VISION_RUN_API RawArchive {
public:
    /** @brief 记下原图根目录 */
    void setRoot(const std::string& rawDir);
    /**
     * @brief 保存这一拍的源图
     * @return 写成的相对说明；失败为空
     */
    std::string save(const std::string& stationId, uint32_t frameSeq, const HsCommData::HsImage& image) const;

private:
    std::string m_root; ///< 原图根目录
};

}

#endif
