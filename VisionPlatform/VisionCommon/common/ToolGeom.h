/**
 * @file ToolGeom.h
 * @brief ToolGeom头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLGEOM_H
#define VISION_TOOLGEOM_H
#include "VisionCommonExport.h"

#include "AlgLibKind.h"
#include "ObjectId.h"
#include "ParamValue.h"
#include <HsCommData/Graphic.h>
#include <HsCommData/HsContour.h>
#include <HsCommData/Geom.h>
#include <HsCommData/HsObject.h>
#include <HsCommData/HsRegion.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Vision {

/** @brief 一条 ROI 的类型。四者互斥，每条只能是其中一种 */
enum class RoiItemKind {
    Undefined = -1, ///< 未定义
    DrawRoi = 0, ///< 绘制 ROI：圆、矩形等标准图形，几何写在 shape 里
    DrawRegion,  ///< 绘制 region：手绘区域，几何写在 shape 里
    RefRoi,      ///< 引用 ROI：保存其他工具的标准图形输出
    RefRegion,    ///< 引用 region：保存其他工具的区域输出
    Max ///< 个数，不是合法值
};

/** @brief 运行时这条 ROI 缺了怎么处理 */
enum class RoiPresence {
    Required = 0, ///< 必须有。引用不到或几何为空，本次工具失败
    Optional      ///< 可有可无。没有就跳过这一条
};

/** @brief 工站、流程、工具、算法共用的一次调度 */
enum class ExecMode {
    Auto = 0, ///< 循环。正式参数
    Manual,   ///< 运行一次
    Step      ///< 单步。只跑指定的一个工具
};

/** @brief 一次执行按哪种方式用学习结果 */
enum class RunMode {
    Auto = 0, ///< 读配置里的学习参数和模板
    Test,     ///< 试跑，不改学习参数，不写模板
    Learn     ///< 允许更新学习参数并写模板
};

/** @class PortLogic @brief 逻辑结果：OK 或 NG */
struct PortLogic {
    bool ok;             ///< true 为 OK，false 为 NG
    bool valid;          ///< 是否有效
    std::string message; ///< 说明
    PortLogic() : ok(false), valid(false) {}
};

/** @brief 一种允许绘制的形状 */
struct RoiShapeRule {
    HsCommData::ObjectKind kind; ///< 几何种类。新形状只在 ObjectKind 末尾追加
    int maxCount;                ///< 该种类最多几块。0 表示不单独限制
    RoiShapeRule() : kind(HsCommData::ObjEmpty), maxCount(0) {}
    RoiShapeRule(HsCommData::ObjectKind k, int n) : kind(k), maxCount(n) {}
};

/** @class RoiSlotRule @brief 一类 ROI 分区：键、界面名、条数和允许的类型 */
struct RoiSlotRule {
    std::string slot;                   ///< 分区键。界面显示用 title
    std::string title;                  ///< 界面名
    int maxCount;                       ///< 该分区最多几条。0 表示不限制
    std::vector<RoiShapeRule> shapes;   ///< 允许绘制的形状。空表示这个分区不能手画
    std::vector<RoiItemKind> itemKinds; ///< 该分区允许的类型，取四种的子集
    RoiSlotRule() : maxCount(0) {}
};

/** @class ToolRoiItem @brief 一条搜索区或检测区。绘制和引用同一结构 */
struct ToolRoiItem {
    std::string slot;              ///< 分区键，对应 RoiSlotRule.slot
    bool enabled;                  ///< 本拍是否启用
    RoiPresence presence;          ///< 必须有，或可有可无
    RoiItemKind kind;              ///< 四选一：绘制 ROI、绘制 region、引用 ROI、引用 region
    HsCommData::HsObject shape;    ///< DrawRoi / DrawRegion 时的几何
    ObjectId refToolId;            ///< RefRoi / RefRegion 时，被引用的工具
    int refIndex;                  ///< 引用该输出的第几块。-1 表示整口
    bool followPose;               ///< 是否用某个工具的位姿做仿射
    ObjectId poseToolId;           ///< followPose 时，提供位姿的工具。每条自己指定
    ToolRoiItem()
        : enabled(true)
        , presence(RoiPresence::Required)
        , kind(RoiItemKind::Undefined)
        , refIndex(-1)
        , followPose(false)
    {}
};

/** @class ResolvedRoi @brief 运行时解析后的一块几何 */
struct ResolvedRoi {
    std::string slot;           ///< 分区键，对应 RoiSlotRule.slot
    HsCommData::HsObject shape; ///< 已解析、已按该条位姿仿射
};

/** @class ToolCaps @brief 工具画布能力。由工具 DLL 填写，平台不按名字猜测 */
struct ToolCaps {
    bool usesRoi;                    ///< 本工具要不要在图上画搜索区
    std::vector<RoiSlotRule> slotRules;  ///< 分区。空表且 usesRoi 为假表示不画
    ToolCaps() : usesRoi(true) {}
    VISION_COMMON_API bool usesMultiRoi() const;

    VISION_COMMON_API int preferredRoiKind() const;

    VISION_COMMON_API bool kindMatches(int kind) const;

};

/** @class JudgeParam @brief 写入配置的一项判断：名称、分组、上下限 */
struct JudgeParam {
    std::string name;   ///< 与本拍测值的键相同
    std::string group;  ///< 分组；可空
    double minVal;      ///< 下限
    double maxVal;      ///< 上限
    bool enable;        ///< 是否参与总判定
    std::string unit;   ///< 单位；可空
    JudgeParam() : minVal(0), maxVal(0), enable(true) {}
};

/**
 * @class ToolParam
 * @brief 写入方案的工具参数：学习、运行、判断三块
 */
/** @brief 示教文件。方案里只存相对路径和内容指纹 */
struct TeachRef {
    std::string relPath;      ///< 相对方案文件的路径
    std::string fingerprint;  ///< 内容指纹
    TeachRef() {}
    bool valid() const { return !relPath.empty(); }
};

/** @brief 一项显示配置 */
struct DisplayCfg {
    bool text;     ///< 文字项
    bool enabled;  ///< 是否显示
    int alpha;     ///< 透明度，0 到 100
    int anchor;    ///< 0 图像角，1 结果点，2 图形旁
    DisplayCfg() : text(false), enabled(true), alpha(100), anchor(0) {}
};

struct ToolParam {
    ParamMap learn;                  ///< 示教参数明文。对应 teachCfg
    ParamMap run;                    ///< 运行参数。配方界面写
    std::vector<JudgeParam> judge;   ///< 判断参数
    TeachRef teachPar;               ///< 示教文件引用
    std::map<std::string, std::string> statCfg; ///< 统计：enable|min|max
    std::map<std::string, DisplayCfg> displayCfg; ///< 显示配置
    std::string backendOverride;     ///< 后端覆盖。空表示跟全局
    AlgLibKind algLib;             ///< 算法库
    ObjectId deviceId;               ///< 绑定设备（采集类）
    std::vector<HsCommData::HsObject> rois;       ///< 界面绘制的搜索区
    std::vector<ToolRoiItem> roiItems; ///< 搜索区记录。绘制和引用同一结构，落盘的是来源
    std::string paraVersion;
    std::string algVersion;
    ToolParam()
        : algLib(AlgLibHal)
        , paraVersion("1.0.0.0")
    {}
};

/** @brief 几何是区域 */
VISION_COMMON_API bool graphicIsRegion(const HsCommData::Graphic& g);
/** @brief 几何是轮廓（不含关键点、骨架） */
VISION_COMMON_API bool graphicIsContour(const HsCommData::Graphic& g);
/** @brief 搜索区转为可拖绘制图元 */
VISION_COMMON_API HsCommData::Graphic roiToDrawing(const HsCommData::HsObject& roi);
/** @brief 绘制图元转为搜索区。箭头、十字、点、文字返回无效 */
VISION_COMMON_API HsCommData::HsObject drawingToRoi(const HsCommData::Graphic& g);
/**
 * @brief 旧存盘字段转 HsObject。箭头、十字、点、文字返回无效
 * kind：0 圆 1 椭圆 2 轴对齐矩形 3 旋转矩形 4 直线 9 轮廓 10 区域
 */
VISION_COMMON_API HsCommData::HsObject toolRoiFromLegacy(int kind, double x, double y, double a, double b, double phi,
                          const std::vector<double>& polyX, const std::vector<double>& polyY,
                          bool polyClosed,
                          const std::vector<std::int32_t>& runRow,
                          const std::vector<std::int32_t>& runCb,
                          const std::vector<std::int32_t>& runCe);
/** @brief 数值里有 x、y 时写成平面位姿。角度按系统设置转入。没有则返回 false */
VISION_COMMON_API bool poseFromValues(const ParamMap& v, HsCommData::Pose2* pose);
/** @brief 平面位姿写入数值。角度按系统设置转出。不写分数 */
VISION_COMMON_API void poseToValues(ParamMap* v, const HsCommData::Pose2& pose);
/**
 * @brief 按示教位姿与当前位姿变换 ROI。先 clone 再改
 * @param rois 就地修改
 */
VISION_COMMON_API void applyPoseToRois(std::vector<HsCommData::HsObject>* rois, const ParamMap& params, const HsCommData::Pose2& pose);
/** @brief 一个图标对象转成图像窗用的叠加。只在显示时调用 */
VISION_COMMON_API HsCommData::Graphic objectToGraphic(const HsCommData::HsObject& obj);
/** @brief 一批图标对象转成图像窗叠加 */
VISION_COMMON_API std::vector<HsCommData::Graphic> objectsToGraphics(const std::vector<HsCommData::HsObject>& objs);

}

#endif
