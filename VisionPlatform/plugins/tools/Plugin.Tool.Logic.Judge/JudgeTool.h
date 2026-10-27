/**
 * @file JudgeTool.h
 * @brief JudgeTool头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JUDGETOOL_H
#define VISION_JUDGETOOL_H

#include "ToolBase.h"
#include "ToolParaJudge.hpp"

namespace Vision {

/**
 * @class JudgeTool
 * @brief 汇总上游 OK/NG，或按引用字段做数值判定
 */
class JudgeTool : public ToolBase {
public:
    /** @brief 注册名 */
    std::string toolName() const;
    /** @brief 工具大类 */
    Vision::ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 工具说明 */
    std::string description() const;
    /** @brief 缺省参数 */
    Vision::ParamMap defaultParams() const;
    /** @brief 参数说明 */
    std::vector<Vision::ParamField> paramFields() const;
    /** @brief 汇总上游结果并按数值条件判定 */
    Vision::ToolOut execute(const Vision::ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    const ToolParaBase* toolParam() const;
private:
    ToolParaJudge m_param;
};

}

#endif
