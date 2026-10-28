/**
 * @file ClassifyTool.h
 * @brief 图像分类工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLASSIFYTOOL_H
#define VISION_CLASSIFYTOOL_H

#include "ToolBase.h"
#include <string>

namespace Vision {

/**
 * @class ClassifyTool
 * @brief 整图分类。模型文件在工具目录，推理在算法插件
 */
class ClassifyTool : public ToolBase {
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
    /** @brief 把外部文件拷进工具目录。界面以后用，不依赖 Qt */
    bool copyIntoToolDir(const std::string& srcPath, const std::string& toolDataDir,
                         const std::string& fileName, std::string* err) const;

private:
    std::vector<ParamField> runFields() const;
    std::vector<ParamField> learnFields() const;
    ParamMap defaults() const;
    std::string textOf(const ToolIn& in, const char* key) const;
    /** @brief 方案未保存或模型、标签文件不在时失败 */
    bool filesReady(const ToolIn& in, std::string* err) const;
    std::string joinToolFile(const ToolIn& in, const std::string& fileName) const;
    bool fileExists(const std::string& path) const;
    void writeLabel(ToolOut* out) const;
};

}

#endif
