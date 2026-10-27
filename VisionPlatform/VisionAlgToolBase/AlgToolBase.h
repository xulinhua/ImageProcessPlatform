/**
 * @file AlgToolBase.h
 * @brief AlgToolBase头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ALGORTOOLBASE_H
#define VISION_ALGORTOOLBASE_H

#include "AlgLibKind.h"
#include "AlgExport.h"
#include "VisionVersion.h"
#include "ToolGeom.h"
#include "ToolIo.h"
#include "ToolParaBase.h"
#include <HsCommData/HsImage.h>
#include <HsCommData/HsObject.h>
#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class AlgToolIn
 * @brief 图像算法输入：像素、几何、运行参数。不含端口，不含方案
 */
struct AlgToolIn {
    RunMode mode;                              ///< 本拍运行模式
    PortImage image;                          ///< 缺省图。同库时 native 可优先
    std::vector<PortImage> images;            ///< 其它图像口
    std::vector<PortRoi> rois;                ///< 本工具搜索区，已跟随位姿
    std::vector<HsCommData::Pose2> poses;               ///< 上游位姿
    std::vector<PortRegion> regions;          ///< 正常区域
    std::vector<PortRegion> masks;            ///< 屏蔽区域
    std::vector<PortContour> contours;        ///< 上游轮廓
    const ToolParaBase* param;                  ///< 工具参数对象，不拥有
    ParamMap run;                              ///< 运行参数
    std::vector<JudgeParam> judge;             ///< 判断参数
    std::string solutionDir;                   ///< 方案目录；可空
    std::string toolDataDir;                   ///< 工具数据目录；可空
    AlgToolIn() : mode(RunMode::Auto), param(nullptr) {}
};


/**
 * @class AlgToolOut
 * @brief 图像算法输出：判定、叠加、数值、图
 */
struct AlgToolOut {
    bool ok;                                   ///< 算法判定
    int errCode;                               ///< 0 表示没有运行失败
    std::vector<HsCommData::HsObject> objects; ///< 结果几何。旧算法仍填这里
    ParamMap values;                           ///< 本拍测值
    PortImage image;                          ///< 缺省输出图
    std::vector<PortImage> images;            ///< 其它输出图
    std::vector<NamedObject> portObjects;      ///< 按口名拆开的几何；可空
    std::vector<HsCommData::Pose2> poses;               ///< 输出位姿
    std::string message;                       ///< 说明
    std::vector<ToolTrace> traces;             ///< 日志和调试记录
    std::vector<ToolInfoItem> infos;           ///< 信息记录，不进对外测值
    AlgToolOut() : ok(false), errCode(0) {}
};
/**
 * @class AlgToolBase
 * @brief 图像算法。按 toolName 挂到已有工具壳上
 */
class VISION_ALGOR_API AlgToolBase {
public:
    virtual ~AlgToolBase() {}
    /** @brief 挂到哪个工具壳，如 locate.find_circle */
    virtual std::string toolName() const = 0;
    /** @brief 算法库 */
    virtual AlgLibKind algLib() const = 0;
    virtual std::string version() const { return std::string("1.0.0.0"); }
    /** @brief 版本变更。只追加，不删旧行 */
    virtual std::vector<VerNote> versionNotes() const;
    /** @brief 像素计算（须可重入） */
    virtual AlgToolOut run(const AlgToolIn& in) = 0;
    /**
     * @brief 克隆本实例
     * @return 新对象，由调用方 destroy
     */
    virtual AlgToolBase* clone() const = 0;
    /** @brief 从工具数据目录装学习结果。缺省空 */
    virtual void loadLearn(const std::string& toolDataDir);
    /** @brief 把学习结果写入工具数据目录。仅 Learn 时调用。缺省空 */
    virtual void saveLearn(const std::string& toolDataDir);
    /** @brief 丢掉内存里的学习句柄。缺省空 */
    virtual void clearLearn();
    /** @brief 在创建本对象的模块内释放（默认 delete this） */
    virtual void destroy();
};

/**
 * @class AlgToolBaseRegistry
 * @brief （toolName, algLib）→ 原型。单例在本 DLL 内
 */
class VISION_ALGOR_API AlgToolBaseRegistry {
public:
    /** @brief 进程内单例 */
    static AlgToolBaseRegistry& instance();
    /** @brief 按 toolName+algLib 登记（接管所有权） */
    void add(AlgToolBase* det);
    /** @brief 兼容：等同 add */
    void set(AlgToolBase* det);
    /** @brief 精确取原型；未登记返回 0 */
    AlgToolBase* get(const std::string& toolName, AlgLibKind algLib) const;
    /** @brief 指定库优先，没有再按 Hal、CV、其余已登记 */
    AlgToolBase* getPreferred(const std::string& toolName, AlgLibKind prefer) const;
    /** @brief 按 Hal、CV 的默认顺序取 */
    AlgToolBase* get(const std::string& toolName) const;
    /** @brief 该工具已登记的算法库 */
    std::vector<AlgLibKind> libs(const std::string& toolName) const;
    /** @brief 释放已登记的算法原型 */
    ~AlgToolBaseRegistry();

private:
    AlgToolBaseRegistry() {}
    std::map<std::string, std::map<AlgLibKind, AlgToolBase*> > m_dets; ///< 本类拥有
};

/** @brief 第一块 ROI；空列表时返回静态空 ROI */
VISION_ALGOR_API const HsCommData::HsObject& firstAlgRoi(const std::vector<HsCommData::HsObject>& rois);

/** @brief 第一块正常搜索区 */
VISION_ALGOR_API const HsCommData::HsObject& firstAlgRoi(const std::vector<PortRoi>& rois);

/** @brief 最后一块位姿。空则返回 false */
VISION_ALGOR_API bool primaryAlgPose(const std::vector<HsCommData::Pose2>& poses, HsCommData::Pose2* out);

/**
 * @brief 按 toolName 调当前算法，填输入并写 ToolOut
 * @param passImage 真则输出图用算法图，假则透传输入图
 */
VISION_ALGOR_API ToolOut runAlgToolBase(const std::string& toolName, const ToolIn& in, bool passImage);

}

#endif
