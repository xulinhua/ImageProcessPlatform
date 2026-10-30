/**
 * @file RstMergeTool.h
 * @brief RstMergeTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RESULTMERGETOOL_H
#define VISION_RESULTMERGETOOL_H

#include "ToolBase.h"
#include "ToolParaRstMerge.hpp"

namespace Vision {

/**
 * @class RstMergeTool
 * @brief 区域合并：登记参数，执行交给算法插件
 */
class RstMergeTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const;
    /** @brief 工具大类 */
    ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 工具说明 */
    std::string description() const;
    /** @brief 缺省参数 */
    ParamMap defaultParams() const;
    /** @brief 参数说明 */
    std::vector<ParamField> paramFields() const;
    /** @brief 画布 ROI 等能力 */
    ToolCaps capabilities() const;
    /** @brief 调算法插件执行 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    /** @brief 本工具参数对象，供参数版本适配与 load 使用 */
    const ToolParaBase* toolParam() const;
private:
    ToolParaRstMerge m_param; ///< 参数快照，ToolBase::adaptParam 时从 ToolParam 载入
};

}

#endif