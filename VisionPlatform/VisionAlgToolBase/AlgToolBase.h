/**
 * @file AlgToolBase.h
 * @brief 图像算法基类
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ALGORTOOLBASE_H
#define VISION_ALGORTOOLBASE_H

#include "AlgLibKind.h"
#include "AlgExport.h"
#include "VisionVersion.h"
#include "elapsed_timer.h"
#include "ToolGeom.h"
#include "ToolIo.h"
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsImage.h>
#include <HsCommData/HsObject.h>
#include <map>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class AlgToolIn
 * @brief 图像算法输入：像素、几何、运行参数。不进端口，不含方案
 */
struct AlgToolIn {
    RunMode mode;                              ///< 本拍运行模式
    PortImage image;                          ///< 缺省图。同库时 native 优先
    std::vector<PortImage> images;            ///< 其它图像
    std::vector<PortRoi> rois;                ///< 本工具搜索区，已跟随位姿
    std::vector<HsCommData::Pose2> poses;               ///< 上游位姿
    std::vector<PortRegion> regions;          ///< 正常区域
    std::vector<PortRegion> masks;            ///< 屏蔽区域
    std::vector<PortContour> contours;        ///< 上游轮廓
    const ToolParaBase* param;                  ///< 工具参数对象，不拥有
    ParamMap run;                              ///< 运行参数
    std::vector<JudgeParam> judge;             ///< 判断参数
    ParamMap teach;                            ///< 示教参数
    const HsCommData::HsImage* imagePtr;       ///< 外部持有的输入图，可空
    NativeRef imageNative;                     ///< 同库原生图
    std::string solutionDir;                   ///< 方案目录，可空
    std::string toolDataDir;                   ///< 工具数据目录，可空
    std::vector<PortCloud> clouds;             ///< 点云
    std::vector<PortPose3> poses3;             ///< 三维位姿
    std::vector<PortShape3> shapes3;           ///< 三维形状
    /** @brief 自动模式，参数对象和外部图指针为空 */
    AlgToolIn() : mode(RunMode::Auto), param(nullptr), imagePtr(nullptr) {}
};

/**
 * @class TolMiss
 * @brief 一项没通过的公差
 */
struct TolMiss {
    std::string name; ///< 公差名
    bool hasValue;    ///< 本拍有没有这个测值
    double value;     ///< 测值
    double minVal;    ///< 下限
    double maxVal;    ///< 上限
    /** @brief 无测值，上下限为 0 */
    TolMiss() : hasValue(false), value(0), minVal(0), maxVal(0) {}
};

/**
 * @class AlgToolOut
 * @brief 图像算法输出：判定、叠加、数值、图
 */
struct AlgToolOut {
    bool ok;                                   ///< 算法判定
    int errCode;                               ///< 0 表示没有运行失败
    std::vector<HsCommData::HsObject> objects; ///< 结果几何。旧算法仍填这里
    ParamMap values;                           ///< 本拍测量
    PortImage image;                          ///< 缺省输出图
    std::vector<PortImage> images;            ///< 其它输出图
    std::vector<NamedObject> portObjects;      ///< 按口名拆开的几何
    std::vector<HsCommData::Pose2> poses;      ///< 输出位姿
    std::vector<char> poseOk;                  ///< 与 poses 对齐。1 表示通过
    std::vector<TolMiss> tolMiss;              ///< 未通过的公差
    HsCommData::Pose2 poseOrg;                 ///< 原始位姿
    bool poseOrgOk;                            ///< 原始位姿是否有效
    bool runOk;                                ///< 算法本身是否跑通
    bool judgeOk;                              ///< 公差是否通过
    double algMs;                              ///< 算法耗时，毫秒
    std::string message;                       ///< 说明
    std::vector<ToolTrace> traces;             ///< 日志和调试记录
    std::vector<ToolInfoItem> infos;           ///< 信息记录，不进端口测量
    std::vector<PortCloud> clouds;             ///< 输出点云
    std::vector<PortPose3> poses3;             ///< 输出三维位姿
    std::vector<PortShape3> shapes3;           ///< 输出三维形状
    std::vector<PortTuple> tuples;             ///< 输出数值元组
    std::vector<ResultLabel> labels;           ///< 图像文字，不进端口
    /** @brief 未判定、未跑通；公差缺省通过 */
    AlgToolOut() : ok(false), errCode(0), poseOrgOk(false), runOk(false), judgeOk(true), algMs(0) {}

    /** @brief 运行失败，不带输出图 */
    static AlgToolOut fail(const std::string& msg)
    {
        AlgToolOut o;
        o.ok = false;
        o.errCode = HS_ERR_OPERATOR;
        o.message = msg;
        return o;
    }
};

/** @brief 本拍输入图：imagePtr 非空优先，其次 image，都没有时返回静态空图 */
inline const HsCommData::HsImage& inputImage(const AlgToolIn& in)
{
    static const HsCommData::HsImage kNone;
    if (in.imagePtr != nullptr && !in.imagePtr->isNull()) {
        return *in.imagePtr;
    }
    if (!in.image.image.isNull()) {
        return in.image.image;
    }
    return kNone;
}

/**
 * @class AlgToolBase
 * @brief 图像算法。按 toolName 挂到已有工具壳上
 */
class VISION_ALGOR_API AlgToolBase {
public:
    /************************************************** 描述 **************************************************/
    /** @brief 虚析构，派生类经 destroy 释放 */
    virtual ~AlgToolBase() {}
    /** @brief 挂到哪一个工具壳，如 locate.find_circle */
    virtual std::string toolName() const = 0;
    /** @brief 注册名。默认转到 toolName */
    virtual std::string tool_name() const;
    /** @brief 算法库。默认转到 algLib */
    virtual AlgLibKind alg_lib() const;
    /** @brief 版本变更。默认转到 versionNotes */
    virtual std::vector<VerNote> version_notes() const;
    /** @brief 算法库 */
    virtual AlgLibKind algLib() const = 0;
    /** @brief 算法版本号，缺省 1.0.0.0 */
    virtual std::string version() const { return std::string("1.0.0.0"); }
    /** @brief 版本变更。只追加，不删旧项 */
    virtual std::vector<VerNote> versionNotes() const;
    /************************************************** 执行 **************************************************/
    /** @brief 像素计算，须可重入 */
    virtual int execute(const AlgToolIn& in, AlgToolOut* out) = 0;
    /** @brief 公差。判定成功后把本拍测值转成公差结果 */
    virtual int judge_tol(const AlgToolIn& in);
    /** @brief 一拍开始前的准备。缺省空 */
    virtual int shot_init(const AlgToolIn& in, AlgToolOut* out);
    /** @brief 一拍结束后的收尾。缺省空 */
    virtual int settle_result(const AlgToolIn& in, AlgToolOut* out);
    /**
     * @brief 一拍完整流程：execute，跑通后 judge_tol，最后 settle_result
     * @param mode 当前未使用
     * @return out->errCode；out 为空时返回 HS_ERR_NULL 或 execute 的错误码
     * @note 写 out->runOk、judgeOk；公差不过时 ok 置 false
     */
    int run(ExecMode mode, const AlgToolIn& in, AlgToolOut* out);
    /**
     * @brief 清空 out 并写成失败
     * @param code 错误码，传 HS_OK 时改记 HS_ERR_OPERATOR
     * @return 实际写入的错误码；out 为空返回 HS_ERR_NULL
     */
    int fail(int code, const std::string& msg, AlgToolOut* out) const;
    /************************************************** 实例与模型 **************************************************/
    /**
     * @brief 克隆一份实例
     * @return 新对象，由调用方 destroy
     */
    virtual AlgToolBase* clone() const = 0;
    /** @brief 从工具数据目录读学习结果。缺省空 */
    virtual void loadLearn(const std::string& toolDataDir);
    /** @brief 把学习结果写入工具数据目录。仅 Learn 时调用，缺省空 */
    virtual void saveLearn(const std::string& toolDataDir);
    /** @brief 丢掉内存里的学习句柄。缺省空 */
    virtual void clearLearn();
    /** @brief 调试动作。未知动作失败 */
    virtual int action(const std::string& id, const AlgToolIn& in, AlgToolOut* out);
    /** @brief 按数据目录装入已保存的模型 */
    virtual void init(const std::string& toolDataDir);
    /** @brief 把当前模型写入数据目录 */
    virtual void save(const std::string& toolDataDir);
    /** @brief 丢掉内存里的模型 */
    virtual void clear(const std::string& toolDataDir);
    /** @brief 按分段写成算法日志，并把合计毫秒写入 out->algMs */
    void write_alg_steps(AlgToolOut* out, const std::vector<Section>& sections, double totalMs) const;
    /** @brief 释放本对象（delete this），跨 DLL 释放时用它而不是直接 delete */
    virtual void destroy();
protected:
    /**
     * @brief 取本工具参数对象。工具壳没传或类型不符时用 in.run、in.judge 填 local
     * @return para 或 local，不会为空
     */
    template <class T>
    const T* paraOf(const AlgToolIn& in, T* local) const
    {
        const T* para = toolParaAs<T>(in.param);
        if (para != nullptr) {
            return para;
        }
        ToolParam src;
        src.run = in.run;
        src.judge = in.judge;
        src.learn = in.teach;
        local->load(src);
        return local;
    }

    SectionTimer run_timer_; ///< execute 分段计时。每次 start 先清空上一拍
};

/**
 * @class AlgToolBaseRegistry
 * @brief (toolName, algLib) 到原型。单例在本 DLL 内
 */
class VISION_ALGOR_API AlgToolBaseRegistry {
public:
    /** @brief 进程内单例 */
    static AlgToolBaseRegistry& instance();
    /** @brief 按 toolName 和 algLib 登记，接管所有权 */
    void add(AlgToolBase* det);
    /** @brief 兼容入口，等同 add */
    void set(AlgToolBase* det);
    /** @brief 精确取原型。未登记返回 0 */
    AlgToolBase* get(const std::string& toolName, AlgLibKind algLib) const;
    /** @brief 指定库优先，没有再按 Hal、CV、其余已登记的库 */
    AlgToolBase* getPreferred(const std::string& toolName, AlgLibKind prefer) const;
    /** @brief 按 Hal、CV 的默认顺序取 */
    AlgToolBase* get(const std::string& toolName) const;
    /** @brief 该工具已登记的算法库 */
    std::vector<AlgLibKind> libs(const std::string& toolName) const;
    /** @brief 释放已登记的算法原型 */
    ~AlgToolBaseRegistry();

private:
    /** @brief 只经 instance 取得 */
    AlgToolBaseRegistry() {}
    std::map<std::string, std::map<AlgLibKind, AlgToolBase*> > m_dets; ///< 按工具名再按库持有
};

/** @brief 第一块 ROI。空列表时返回静态空 ROI */
VISION_ALGOR_API const HsCommData::HsObject& firstAlgRoi(const std::vector<HsCommData::HsObject>& rois);

/** @brief 第一块正常搜索区 */
VISION_ALGOR_API const HsCommData::HsObject& firstAlgRoi(const std::vector<PortRoi>& rois);

/** @brief 最后一块位姿。空则返回 false */
VISION_ALGOR_API bool primaryAlgPose(const std::vector<HsCommData::Pose2>& poses, HsCommData::Pose2* out);

/**
 * @brief 按 toolName 调当前算法，填输入并写 ToolOut
 * @param passImage 为真时输出图用算法图，为假时透传输入图
 */
VISION_ALGOR_API ToolOut runAlgToolBase(const std::string& toolName, const ToolIn& in, bool passImage);

}

#endif