/**
 * @file CalibPlateTool.h
 * @brief 标定板。学习时导入已算好的相机模型
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIBPLATETOOL_H
#define VISION_CALIBPLATETOOL_H

#include "ToolBase.h"
#include "AlgToolBase.h"
#include "ToolParaCalibPlate.hpp"

namespace Vision {

/**
 * @class CalibPlateTool
 * @brief 标定板。学习时导入已算好的相机模型
 * @note CalibMgr 提交/落盘仍在 execute；算法板算走 attachAlg + shot
 */
class CalibPlateTool : public ToolBase {
public:
    CalibPlateTool();
    ~CalibPlateTool() override;
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
    std::vector<PortDesc> cfg_port() const override;

    int execute(const ToolIn& in, ToolOut* out) override;
    int save() override;
    void clear(const std::string& toolDataDir) override;

private:
    void fill_alg_in(const ToolIn& in, Vision::AlgToolIn* ain) const;
    int shot_learn(const ToolIn& in, ToolOut* out);

    ToolParaCalibPlate param_;

private:
    std::string toolName() const override { return tool_name(); }
    std::string displayName() const override { return display_name(); }
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
    ParamMap defaultParams() const override { return default_params(); }
    std::vector<ParamField> paramFields() const override { return cfg_param(); }
    ToolCaps capabilities() const override { return cfg_roi(); }
    std::vector<PortDesc> ports() const override { return cfg_port(); }
    const ToolParaBase* toolParam() const override { return tool_param(); }
    ToolOut execute(const ToolIn& in) override;
};

}

#endif
