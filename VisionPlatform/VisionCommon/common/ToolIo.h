/**
 * @file ToolIo.h
 * @brief ToolIo头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLIO_H
#define VISION_TOOLIO_H
#include "VisionCommonExport.h"

#include "ObjectId.h"
#include "PortTypes.h"
#include "AlgLibKind.h"
#include "ToolGeom.h"
#include <HsCommData/HsImage.h>
#include <HsCommData/HsObject.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/Graphic.h>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Vision {

/** @brief 日志还是调试记录。数值与 LogKind 一致 */
enum ToolTraceKind {
    ToolTraceLog = 0, ///< 操作日志
    ToolTraceDebug    ///< 调试记录
};


/** @class PortDesc @brief 工具端口：槽位与序列化键 */
struct VISION_COMMON_API PortDesc {
    PortSlot slot; ///< 方向、数据类型、口名
    PortDesc() {}
    PortDesc(PortDir d, PortData t) : slot(d, t) {}
    /** @brief 由槽位生成 in.xxx / out.xxx，有口名时带上口名 */
    std::string key() const;
    PortDir dir() const;
    PortData data() const;
};

/**
 * @class NativeRef
 * @brief 同库流转用的算法库对象。不拥有，不写入配置
 */
struct NativeRef {
    AlgLibKind algLib; ///< 原生对象所属库
    void* ptr;           ///< 同库自行转换；跨库和界面只用旁边的 Hs 对象
    NativeRef() : algLib(AlgLibHal), ptr(nullptr) {}
};

/** @class PortImage @brief 带口名的一张图 */
struct PortImage {
    std::string port;              ///< 口名；空为缺省口
    int index;                     ///< 该口上的序号
    HsCommData::HsImage image;     ///< 共用图像
    NativeRef native;              ///< 同库指针，可空
    PortImage() : index(0) {}
};

/** @class PortRegion @brief 带口名和用途的一块区域 */
struct PortRegion {
    std::string port;              ///< 口名；空为缺省口
    int index;                     ///< 该口上的序号
    RegionUse use;                 ///< 正常、屏蔽或无效
    HsCommData::HsObject object;   ///< 共用区域
    NativeRef native;              ///< 同库指针，可空
    PortRegion() : index(0), use(RegionUse::Normal) {}
};

/** @class PortContour @brief 带口名的一条轮廓 */
struct PortContour {
    std::string port;              ///< 口名；空为缺省口
    int index;                     ///< 该口上的序号
    HsCommData::HsObject object;   ///< 共用轮廓
    NativeRef native;              ///< 同库指针，可空
    PortContour() : index(0) {}
};

/** @class PortRoi @brief 带口名和用途的一块搜索区 */
struct PortRoi {
    std::string port;  ///< 口名；空为缺省口
    int index;         ///< 该口上的序号
    RegionUse use;     ///< 正常、屏蔽或无效
    HsCommData::HsObject roi;       ///< 搜索区
    PortRoi() : index(0), use(RegionUse::Normal) {}
};

/** @class PortPose @brief 带口名的一个位姿 */
struct PortPose {
    std::string port;  ///< 口名；空为缺省口
    int index;         ///< 该口上的序号
    HsCommData::Pose2 pose;     ///< 位姿
    PortPose() : index(0) {}
};

/** @class PortTuple @brief 带口名的一个数值元组 */
struct PortTuple {
    std::string port;       ///< 口名；空为缺省口
    int index;              ///< 该口上的序号
    HsCommData::HsTuple value; ///< 数值
    PortTuple() : index(0) {}
};

/** @class NamedObject @brief 算法输出的一只几何，可挂同库指针 */
struct NamedObject {
    std::string port;            ///< 口名；空为缺省口
    int index;                   ///< 该口上的序号
    HsCommData::HsObject object; ///< 共用几何
    NativeRef native;            ///< 同库指针，可空
    NamedObject() : index(0) {}
};


/**
 * @class ToolIo
 * @brief 工具层一份输入或输出。按口名存放，一类一个数组
 */
struct VISION_COMMON_API ToolIo {
    std::vector<PortImage> images;     ///< 图像
    std::vector<PortRegion> regions;   ///< 区域
    std::vector<PortContour> contours; ///< 轮廓
    std::vector<PortRoi> rois;         ///< 搜索区
    std::vector<PortLogic> logics;      ///< 逻辑结果
    std::vector<PortPose> poses;       ///< 位姿
    std::vector<PortTuple> tuples;     ///< 数值元组

    /** @brief 分类数据转画面叠加。只给图像窗。无效项不画 */
    std::vector<HsCommData::Graphic> toGraphics() const;
    /** @brief 绘制图元里的几何拆进区域、轮廓、ROI */
    void appendFromGraphics(const std::vector<HsCommData::Graphic>& gfx);
    /** @brief 按种类把图标对象拆进区域、轮廓、ROI */
    void appendObjects(const std::vector<HsCommData::HsObject>& objs);
    /** @brief 按端口类型把 src 中缺省口的全部项追加过来 */
    void appendByType(const ToolIo& src, PortData kind);
    /**
     * @brief 按连线从 src 收一项或整口
     * @param index 小于 0 表示整口
     * @param use 写到区域和 ROI 上；其它类型忽略
     */
    void appendByType(const ToolIo& src, const PortSlot& fromPort, const PortSlot& toPort, int index, RegionUse use);
};

/**
 * @class ToolIn
 * @brief execute 入参。调度填好，工具看不见方案、工站、流程
 */
class ToolParaBase;

struct ToolIn {
    RunMode mode;                                      ///< 本拍运行模式
    const ToolParaBase* toolParam;                      ///< 本拍参数对象，不拥有
    HsCommData::HsImage input;                         ///< 缺省输入图
    NativeRef inputNative;                             ///< 缺省图的同库指针，可空
    ToolIo inputIo;                                    ///< 按连线收好的上游数据
    ToolParam param;                                     ///< 学习、运行、判断的本拍副本
    std::string solutionDir;                           ///< 方案目录；未保存可空
    std::string toolDataDir;                           ///< 本工具数据目录；未保存可空
    AlgLibKind algLib;                               ///< 本拍选用的算法库
    std::vector<HsCommData::HsObject> rois;            ///< 本工具自己的搜索区
    bool ownRoisResolved;                              ///< 本工具 ROI 已按分区解析，不再整表跟随位姿
    std::vector<PortRoi> ownRois;                      ///< 解析后的标准图形，port 为分区
    std::vector<PortRegion> ownRegions;                ///< 解析后的区域，port 为分区
    ParamMap runtime;                                  ///< ToolCfg.runtime 的副本
    ParamMap upstream;                                 ///< 上游数值。Tuple 口接上之前判断工具仍读
    std::map<std::string, ParamMap> refValues;         ///< 其他工具数值，键为工具 id
    std::map<std::string, std::string> refNames;       ///< 其他工具显示名，键为工具 id
    ToolIn() : mode(RunMode::Auto), toolParam(nullptr), algLib(AlgLibHal), ownRoisResolved(false) {}
};


/**
 * @class ToolTrace
 * @brief 工具或算法的一条日志、调试记录。用户和工站由流程汇总时补上
 */
struct ToolTrace {
    int level;            ///< 与 LogLevel 相同：0 跟踪到 4 错误
    ToolTraceKind kind;   ///< 操作日志或调试记录
    std::string action;   ///< 短键
    std::string detail;   ///< 正文
    ToolTrace() : level(2), kind(ToolTraceLog) {}
};

/**
 * @class ToolInfoItem
 * @brief 算法内部明细。不进对外 values
 */
struct ToolInfoItem {
    std::string name; ///< 这组明细的名字
    ParamMap fields;  ///< 数量、面积、长、宽等
};

/**
 * @class ToolOut
 * @brief 一次 execute 的输出
 */
struct VISION_COMMON_API ToolOut {
    ObjectId toolId;                                ///< 工具 id（调度回填）
    int64_t frameId;                                ///< 帧号
    bool ok;                                        ///< 判定。NG 时仍可为 false 且 errCode 为 0
    bool error;                                     ///< 运行异常（非普通 NG）
    int errCode;                                    ///< 0 表示没有运行失败
    int elapsedMs;                                  ///< 耗时
    HsCommData::HsImage image;                      ///< 缺省输出图
    NativeRef imageNative;                          ///< 缺省输出图的同库指针，可空
    std::vector<HsCommData::HsObject> objects;      ///< 结果几何，显示前再转 Graphic
    std::string message;                            ///< 说明
    ParamMap values;                                ///< 本拍测值，键与 JudgeParam.name 对应
    HsCommData::HsImage source;                     ///< 源图
    ToolIo io;                                      ///< 分类后的工具层数据
    ParamMap runtime;                               ///< 写回 ToolCfg.runtime；空则调度不覆盖
    std::vector<ToolTrace> traces;                  ///< 日志和调试记录
    std::vector<ToolInfoItem> infos;                ///< 信息记录，不进端口
    ToolOut() : frameId(0), ok(false), error(false), errCode(0), elapsedMs(0) {}

    /** @brief 补齐 io 与 image、objects、ok 互相同步 */
    void syncIo();
};


/** @brief 正常搜索区，丢掉屏蔽和无效 */
VISION_COMMON_API std::vector<HsCommData::HsObject> plainRois(const std::vector<PortRoi>& items);

/** @brief 正常区域对象 */
VISION_COMMON_API std::vector<HsCommData::HsObject> plainRegions(const std::vector<PortRegion>& items);

/** @brief 轮廓对象 */
VISION_COMMON_API std::vector<HsCommData::HsObject> plainContours(const std::vector<PortContour>& items);

/** @brief 位姿列表 */
VISION_COMMON_API std::vector<HsCommData::Pose2> plainPoses(const std::vector<PortPose>& items);

}

#endif
