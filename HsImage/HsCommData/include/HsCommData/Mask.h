/**
 * @file Mask.h
 * @brief Mask头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_MASK_H
#define HSCOMMDATA_MASK_H

#include <HsCommData/HsCommDataExport.h>

#include <cstdint>
#include <vector>

namespace HsCommData {

/**
 * @class MaskP
 * @brief 与底图同尺寸的 8bit 掩码。像素 0 为透明，不含颜色
 */
struct HSCOMMDATA_EXPORT MaskP {
    int32_t width;                     ///< 宽
    int32_t height;                    ///< 高
    std::vector<unsigned char> pixels; ///< 行优先，size = width*height

    MaskP(); ///< 空掩码
    /** @brief 同类型拷贝，像素缓冲整份复制 */
    MaskP(const MaskP& other);
    /** @brief 同类型赋值，像素缓冲整份复制 */
    MaskP& operator=(const MaskP& other);
    /** @brief 尺寸或缓冲非法则为空 */
    bool isEmpty() const;
};

}

#endif
