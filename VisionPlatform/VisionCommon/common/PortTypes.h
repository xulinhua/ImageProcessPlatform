/**
 * @file PortTypes.h
 * @brief PortTypes头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PORTTYPES_H
#define VISION_PORTTYPES_H

#include "VisionCommonExport.h"
#include <string>

namespace Vision {

/** @brief 工具端口方向 */
enum class PortDir {
    In = 0, ///< 输入
    Out     ///< 输出
};

/** @brief 工具端口数据类型（均可为向量）。新类型只追加在末尾，避免已存盘的整数错位 */
enum class PortData {
    Image = 0,  ///< 图像
    Region,     ///< 区域
    Contour,    ///< 轮廓
    Roi,        ///< 标准图形（圆、矩形、直线等）
    Logic,      ///< 逻辑结果（OK/NG）
    Pose,       ///< 位姿（x、y、角度）
    Tuple,       ///< 数值元组
    Cloud,       ///< 点云
    Pose3,       ///< 三维位姿
    Shape3,      ///< 三维形状
    Max ///< 个数，不是合法值
};

/** @brief 区域或 ROI 在本拍里怎么用 */
enum class RegionUse {
    Normal = 0, ///< 正常区域，参与搜索或检测
    Mask,       ///< 屏蔽区域，从正常区域里扣除
    Invalid     ///< 无效区域，本拍不用
};

/** @class PortSlot @brief 端口槽位：方向、数据类型、口名 */
struct PortSlot {
    PortDir dir;       ///< 入或出
    PortData data;     ///< 数据类型
    std::string name;  ///< 口名；空表示该类型的缺省口
    /** @brief 缺省图像输出口 */
    PortSlot() : dir(PortDir::Out), data(PortData::Image) {}
    /** @brief 指定方向和类型的缺省口 */
    PortSlot(PortDir d, PortData t) : dir(d), data(t) {}
    /** @brief 方向、类型、口名都相同 */
    bool operator==(const PortSlot& o) const { return dir == o.dir && data == o.data && name == o.name; }
    /** @brief 任一字段不同 */
    bool operator!=(const PortSlot& o) const { return !(*this == o); }
};

/** @brief 构造输入端口槽位 */
inline PortSlot portSlotIn(PortData d) { return PortSlot(PortDir::In, d); }
/** @brief 构造输出端口槽位 */
inline PortSlot portSlotOut(PortData d) { return PortSlot(PortDir::Out, d); }

/** @brief 整数还原区域用途；越界当正常区域 */
    VISION_COMMON_API RegionUse regionUseFromInt(int v);


/** @brief 数据类型对应的键后缀 */
    VISION_COMMON_API const char* portDataSuffix(PortData d);


/** @brief 端口槽位转 in.xxx / out.xxx；有口名时再加一段 */
    VISION_COMMON_API std::string portKey(const PortSlot& s);


/** @brief 端口名规范成 in.xxx、out.xxx；兼容旧 graphics、values */
    VISION_COMMON_API std::string canonicalizePortKey(const std::string& key);


/** @brief 由类型后缀解析数据类型 */
    VISION_COMMON_API PortData portDataFromSuffix(const std::string& k);


/** @brief 从键解析端口槽位。in.image.locate 的口名是 locate */
    VISION_COMMON_API PortSlot portSlotFromKey(const std::string& key);


/** @brief 由端口名判断数据类型 */
inline PortData portDataFromKey(const std::string& key)
{
    return portSlotFromKey(key).data;
}

/** @brief 两端端口是否同一数据类型且方向正确。口名可以不同 */
inline bool portsTypeMatch(const PortSlot& from, const PortSlot& to)
{
    return from.dir == PortDir::Out && to.dir == PortDir::In && from.data == to.data;
}

/** @brief 由存盘的方向与数据类型整数还原端口槽位 */
    VISION_COMMON_API PortSlot portSlotFromInts(int dir, int data);


}

#endif
