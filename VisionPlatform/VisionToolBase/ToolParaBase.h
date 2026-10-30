/**
 * @file ToolParaBase.h
 * @brief ToolParaBase头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARABASE_H
#define VISION_TOOLPARABASE_H

#include "ToolExport.h"
#include "ToolGeom.h"
#include <cstring>
#include <string>
#include <vector>

namespace Vision {

/** @brief 参数适配方向 */
enum AdaptDir {
    AdaptUpgrade = 0,  ///< 旧版本升到新版本，按 fromVer→toVer 执行步进
    AdaptDowngrade = 1 ///< 新版本退回旧版本，按 toVer→fromVer 反向执行步进
};

/**
 * @class ParaAdaptStep
 * @brief 一步参数布局适配：fromVer 与 toVer 之间的转换
 */
struct ParaAdaptStep {
    const char* fromVer; ///< 这一步的起点
    const char* toVer;   ///< 这一步的终点
    bool needConfirm;    ///< 自动执行时要先人工确认
    /** @brief 空步骤：起止版本为空串，fromVer 为空的步骤会被跳过 */
    ParaAdaptStep() : fromVer(""), toVer(""), needConfirm(false) {}
};

/**
 * @class ToolParaBase
 * @brief 运行时工具参数基类。学习、运行由子类自己的结构体承担
 */
class VISION_TOOL_API ToolParaBase {
public:
    /** @brief 虚析构 */
    virtual ~ToolParaBase() {}
    /** @brief 从落盘的 ToolParam 填写本对象 */
    virtual void load(const ToolParam& src) = 0;
    /** @brief 参数类型名。跨模块识别，空串表示未登记 */
    virtual const char* typeName() const { return ""; }
    /** @brief 本参数布局的版本号，缺省 1.0.0.0；adaptParam 的 newVer 为空时以它为目标 */
    virtual const char* paraVersion() const { return "1.0.0.0"; }
    /**
     * @brief 把 src 从 oldVer 连续适配到 newVer，并写回 src->paraVersion
     * 空版本和 0.0.0.0 当成 1.0.0.0。没有登记步进且两端相同则直接通过。
     */
    bool adaptParam(ToolParam* src, const std::string& oldVer, const std::string& newVer, std::string* detail);
    /** @brief 列出将要走的步进名。needConfirm 任一步需要确认则为 true */
    void preview(const std::string& fromVer, const std::string& toVer,
                 std::vector<std::string>* names, bool* needConfirm) const;
    /**
     * @brief 逐段比较两个四段版本号；缺段按 0，空串按 0.0.0.0（不像 adaptParam 那样当成 1.0.0.0）
     * @return a 小于、等于、大于 b 时分别为负、0、正
     */
    static int compareVersion(const std::string& a, const std::string& b);

    std::vector<JudgeParam> judge; ///< 判断参数，从 ToolParam 原样拷贝

protected:
    /** @brief 本工具登记的参数步进数。没有改过参数布局则为 0 */
    virtual int adaptStepCount() const { return 0; }
    /** @brief 第 index 步的起止版本。越界返回空步骤 */
    virtual ParaAdaptStep adaptStepAt(int index) const;
    /**
     * @brief 执行第 index 步。子类在这里调用自己的 AdaptPara_xxxxxxxx_yyyyyyyy
     * 这些函数留在子类里，不导出。
     */
    virtual bool applyAdaptStep(int index, ToolParam* param, AdaptDir dir);
};

/** @brief 按类型名取出子类参数。不符返回 0 */
template <class T>
const T* toolParaAs(const ToolParaBase* param)
{
    if (param == nullptr || std::strcmp(param->typeName(), T::typeId()) != 0) {
        return nullptr;
    }
    return static_cast<const T*>(param);
}

}

#endif