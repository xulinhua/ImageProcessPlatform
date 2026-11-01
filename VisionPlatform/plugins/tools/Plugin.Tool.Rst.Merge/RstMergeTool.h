/**
 * @file RstMergeTool.h
 * @brief 区域合并：登记参数，执行交给算法插件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RESULTMERGETOOL_H
#define VISION_RESULTMERGETOOL_H

#include "ToolBase.h"
#include "AlgToolBase.h"
#include "ToolParaRstMerge.hpp"

namespace Vision {

class RstMergeTool : public ToolBase {
public:
    RstMergeTool();
    ~RstMergeTool() override;
    ToolBase* clone() const override;

    std::string tool_name() const override;
    ToolType type() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::string version() const override;
    std::vector<VerNote> version_notes() const override;

    ParamMap default_params() const override;
    std::vector<ParamField> cfg_param() const override;
    const ToolParaBase* tool_param() const override;
    int check_param(std::string* detail) const override;

    ToolCaps cfg_roi() const override;

    int execute(const ToolIn& in, ToolOut* out) override;

private:
    static std::vector<ParamField> param_fields();
    static ToolCaps roi_caps();
    void fill_alg_in(const ToolIn& in, AlgToolIn* ain) const;
    int shot(const ToolIn& in, const std::string& actionId, ToolOut* out);

    ToolParaRstMerge param_;

private:
    std::string toolName() const override { return tool_name(); }
    std::string displayName() const override { return display_name(); }
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
    ParamMap defaultParams() const override { return default_params(); }
    std::vector<ParamField> paramFields() const override { return cfg_param(); }
    ToolCaps capabilities() const override { return cfg_roi(); }
    const ToolParaBase* toolParam() const override { return tool_param(); }
    ToolOut execute(const ToolIn& in) override;
};

}

#endif
