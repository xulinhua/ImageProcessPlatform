/**
 * @file OcrTool.h
 * @brief 字符识别工具
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_OCRTOOL_H
#define VISION_OCRTOOL_H

#include "ToolBase.h"
#include "ToolParaOcr.hpp"

namespace Vision {

/**
 * @class OcrTool
 * @brief 字符识别：登记参数，执行交给算法插件
 */
class OcrTool : public ToolBase {
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
    /** @brief 搜索区 */
    ToolCaps capabilities() const;
    /** @brief 调算法插件执行 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    /** @brief 本工具参数对象 */
    const ToolParaBase* toolParam() const;

private:
    /** @brief 可选字体。Halcon 自带分类器 */
    std::vector<std::string> fonts() const;

    ToolParaOcr m_param; ///< 本拍参数
};

}

#endif
