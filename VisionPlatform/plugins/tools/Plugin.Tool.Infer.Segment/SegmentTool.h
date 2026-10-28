/**
 * @file SegmentTool.h
 * @brief 语义分割工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_SEGMENTTOOL_H
#define VISION_SEGMENTTOOL_H

#include "ToolBase.h"
#include <string>

namespace Vision {

/** @class SegmentTool @brief 语义分割。每个前景类一块区域 */
class SegmentTool : public ToolBase {
public:
    std::string toolName() const;
    ToolType type() const;
    std::string displayName() const;
    std::string version() const;
    std::vector<VerNote> versionNotes() const;
    std::string description() const;
    ParamMap defaultParams() const;
    std::vector<ParamField> paramFields() const;
    std::vector<ParamField> cfg_teach() const;
    std::vector<PortDesc> ports() const;
    ToolOut execute(const ToolIn& in);
    ToolBase* clone() const;
    bool copyIntoToolDir(const std::string& srcPath, const std::string& toolDataDir,
                         const std::string& fileName, std::string* err) const;

private:
    std::vector<ParamField> learnFields() const;
    std::vector<ParamField> runFields() const;
    std::string textOf(const ToolIn& in, const char* key) const;
    bool filesReady(const ToolIn& in, std::string* err) const;
    std::string joinToolFile(const ToolIn& in, const std::string& fileName) const;
    bool fileExists(const std::string& path) const;
};

}
#endif
