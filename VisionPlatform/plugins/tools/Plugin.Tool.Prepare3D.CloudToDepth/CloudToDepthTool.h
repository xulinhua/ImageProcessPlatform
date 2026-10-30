/**
 * @file CloudToDepthTool.h
 * @brief 点云转深度图工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLOUDTODEPTHTOOL_H
#define VISION_CLOUDTODEPTHTOOL_H

#include "ToolBase.h"

namespace Vision {

class CloudToDepthTool : public ToolBase {
public:
    std::string toolName() const;
    ToolType type() const;
    std::string displayName() const;
    std::string version() const;
    std::vector<VerNote> versionNotes() const;
    std::string description() const;
    ParamMap defaultParams() const;
    std::vector<ParamField> paramFields() const;
    std::vector<PortDesc> ports() const;
    ToolOut execute(const ToolIn& in);
    ToolBase* clone() const;
};

}

#endif