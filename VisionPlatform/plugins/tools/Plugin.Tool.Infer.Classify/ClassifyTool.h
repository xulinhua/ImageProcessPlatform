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
    /************************************************** 身份 **************************************************/
    /** @brief 注册名 infer.classify */
    std::string toolName() const;
    /** @brief 工具大类：推理 */
    ToolType type() const;
    /** @brief 界面显示名 */
    std::string displayName() const;
    /** @brief 工具版本 */
    std::string version() const;
    /** @brief 版本变更，只追加 */
    std::vector<VerNote> versionNotes() const;
    /** @brief 工具说明 */
    std::string description() const;

    /************************************************** 参数 **************************************************/
    /** @brief 缺省参数，同 defaults() */
    ParamMap defaultParams() const;
    /** @brief 全部参数说明：示教字段在前，运行字段在后 */
    std::vector<ParamField> paramFields() const;
    /** @brief 示教参数说明：模型文件、类别文件 */
    std::vector<ParamField> cfg_teach() const;
    /** @brief 端口：入图像；出 classIndex、score、className 和逻辑口 */
    std::vector<PortDesc> ports() const;

    /************************************************** 执行 **************************************************/
    /** @brief 文件齐全时强制走 ONNX 推理，结果文字贴在图左上角 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    /**
     * @brief 把外部文件拷进工具目录。界面以后用，不依赖 Qt
     * @param srcPath 源文件，UTF-8
     * @param toolDataDir 工具数据目录，空表示方案未保存
     * @param fileName 目标文件名
     * @param err 失败原因，可空
     * @return 拷贝成功返回 true，已有同名文件会覆盖
     */
    bool copyIntoToolDir(const std::string& srcPath, const std::string& toolDataDir,
                         const std::string& fileName, std::string* err) const;

private:
    /************************************************** 参数 **************************************************/
    /** @brief 运行参数说明：输入宽高（像素）、分数阈值 0~1 */
    std::vector<ParamField> runFields() const;
    /** @brief 示教参数说明：model 缺省 model.onnx，labels 缺省 labels.txt */
    std::vector<ParamField> learnFields() const;
    /** @brief 按 paramFields 缺省值拼 ParamMap */
    ParamMap defaults() const;
    /** @brief 取文本参数，learn 里有就读 learn，否则读 run */
    std::string textOf(const ToolIn& in, const char* key) const;

    /************************************************** 文件 **************************************************/
    /** @brief 方案未保存或模型、标签文件不在时失败 */
    bool filesReady(const ToolIn& in, std::string* err) const;
    /** @brief 工具数据目录拼文件名 */
    std::string joinToolFile(const ToolIn& in, const std::string& fileName) const;
    /** @brief 路径存在且是文件（UTF-8 路径） */
    bool fileExists(const std::string& path) const;

    /************************************************** 输出 **************************************************/
    /** @brief 没有标签时用 className 和 score 补一条，贴在 (8,8) */
    void writeLabel(ToolOut* out) const;
};

}

#endif
