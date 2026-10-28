/**
 * @file ParamKeys.h
 * @brief ParamKeys头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PARAMKEYS_H
#define VISION_PARAMKEYS_H
#include "VisionCommonExport.h"

#include "ParamValue.h"
#include <string>

namespace Vision {

/** @brief 算法输出 values 中的标准键 */
enum class ValueKey {
    X = 0,     ///< 列 / 圆心 x
    Y,         ///< 行 / 圆心 y
    Cx,        ///< 圆心列（别名）
    Cy,        ///< 圆心行（别名）
    Angle,     ///< 角度（弧度）
    Score,     ///< 匹配分数
    Ok,        ///< 判定 OK
    Pass,      ///< 判定通过（别名）
    R          ///< 半径
};

/** @brief 工具运行态 runtime 中的标准键 */
enum class RuntimeKey {
    Index = 0,     ///< 文件列表索引
    LastFile,      ///< 上次抓取文件路径
    GrabPath,      ///< 抓取目录
    LastX,         ///< 上次结果 x
    LastY,         ///< 上次结果 y
    LastA,         ///< 上次结果角度
    LastScore,     ///< 上次结果分数
    PathChanged    ///< 路径是否变更
};

/** @brief 示教/跟随 params 中的标准键 */
enum class TeachKey {
    RefX = 0, ///< 示教位姿 x
    RefY,     ///< 示教位姿 y
    RefA      ///< 示教位姿角度
};

/** @brief ValueKey 序列化名 */
VISION_COMMON_API const char* valueKeyName(ValueKey k);
/** @brief 从序列化名解析 ValueKey；未知返回 false */
VISION_COMMON_API bool valueKeyFromName(const std::string& s, ValueKey* out);
/** @brief RuntimeKey 序列化名 */
VISION_COMMON_API const char* runtimeKeyName(RuntimeKey k);
/** @brief 从序列化名解析 RuntimeKey；未知返回 false */
VISION_COMMON_API bool runtimeKeyFromName(const std::string& s, RuntimeKey* out);
/** @brief TeachKey 序列化名 */
VISION_COMMON_API const char* teachKeyName(TeachKey k);
/** @brief 从序列化名解析 TeachKey；未知返回 false */
VISION_COMMON_API bool teachKeyFromName(const std::string& s, TeachKey* out);

/** @brief ParamMap 是否含 ValueKey */
VISION_COMMON_API bool paramHas(const ParamMap& m, ValueKey k);
/** @brief 读取 ValueKey；缺键返回 def */
VISION_COMMON_API ParamValue paramGet(const ParamMap& m, ValueKey k, const ParamValue& def = ParamValue());
/** @brief 写入 ValueKey */
VISION_COMMON_API void paramSet(ParamMap* m, ValueKey k, const ParamValue& v);

/** @brief ParamMap 是否含 RuntimeKey */
VISION_COMMON_API bool runtimeHas(const ParamMap& m, RuntimeKey k);
/** @brief 读取 RuntimeKey */
VISION_COMMON_API ParamValue runtimeGet(const ParamMap& m, RuntimeKey k, const ParamValue& def = ParamValue());
/** @brief 写入 RuntimeKey */
VISION_COMMON_API void runtimeSet(ParamMap* m, RuntimeKey k, const ParamValue& v);

/** @brief ParamMap 是否含 TeachKey */
VISION_COMMON_API bool teachHas(const ParamMap& m, TeachKey k);
/** @brief 读取 TeachKey */
VISION_COMMON_API ParamValue teachGet(const ParamMap& m, TeachKey k, const ParamValue& def = ParamValue());
/** @brief 写入 TeachKey */
VISION_COMMON_API void teachSet(ParamMap* m, TeachKey k, const ParamValue& v);

}

#endif
