/**
 * @file AlgLibKind.h
 * @brief AlgLibKind头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ALGOLIBKIND_H
#define VISION_ALGOLIBKIND_H
#include "VisionCommonExport.h"

#include <string>

namespace Vision {

/**
 * @brief 算法库种类（与插件 algLib() 登记名对应；UI 列名用短标题）
 * @note 新增时同步 AlgLibKindMax 与注册名表
 */
enum AlgLibKind {
    AlgLibHal = 0,   ///< Halcon，登记名 halcon，列名 Hal
    AlgLibHik = 1,   ///< 海康，登记名 hik，列名 Hik
    AlgLibHray = 2,  ///< 华睿等，登记名 hray，列名 Hray
    AlgLibCv = 3,    ///< OpenCV，登记名 opencv，列名 CV
    AlgLibPcl = 4,   ///< PCL，登记名 pcl，列名 PCL
    AlgLibOpen3d = 5,///< Open3D，登记名 open3d，列名 Open3D。本机先不放插件
    AlgLibOnnx = 6,  ///< ONNX Runtime，登记名 onnx，列名 Onnx
    AlgLibKindMax ///< 种类个数（哨兵，不作合法值）
};

/** @brief UI/表头短标题：Hal / Hik / Hray / CV；非法返回空串 */
VISION_COMMON_API const char* algLibKindTitle(AlgLibKind k);
/** @brief 插件登记名：halcon / hik / hray / opencv；非法返回空串 */
VISION_COMMON_API const char* algLibKindRegName(AlgLibKind k);
/**
 * @brief 登记名转枚举（大小写不敏感；兼容 cv→opencv）
 * @return 成功为 true
 */
VISION_COMMON_API bool algLibKindFromRegName(const std::string& regName, AlgLibKind* out);
/** @brief 下标是否为合法 AlgLibKind（不含 Count） */
VISION_COMMON_API bool algLibKindValid(int k);

}

#endif