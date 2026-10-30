/**
 * @file DetectTool.h
 * @brief 目标检测工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DETECTTOOL_H
#define VISION_DETECTTOOL_H

#include "ToolBase.h"
#include <string>

namespace Vision {

/**
 * @class DetectTool
 * @brief 目标检测。模型文件在工具目录，框和分数由 ONNX 算法插件填写
 */
class DetectTool : public ToolBase {
public:
    /************************************************** 身份 **************************************************/
    /** @brief 注册名 infer.detect */
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
    /** @brief 按 paramFields 缺省值拼 ParamMap */
    ParamMap defaultParams() const;
    /** @brief 全部参数说明：示教字段在前，运行字段在后 */
    std::vector<ParamField> paramFields() const;
    /** @brief 示教参数说明：模型文件、类别文件 */
    std::vector<ParamField> cfg_teach() const;
    /** @brief 端口：入图像；出框（Roi）、classIndex、score 和逻辑口 */
    std::vector<PortDesc> ports() const;

    /************************************************** 执行 **************************************************/
    /** @brief 文件齐全时强制走 ONNX 推理，输出图沿用输入图 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    /**
     * @brief 把外部文件拷进工具目录
     * @param srcPath 源文件，UTF-8
     * @param toolDataDir 工具数据目录，空表示方案未保存
     * @param fileName 目标文件名
     * @param err 失败原因，可空
     * @return 拷贝成功返回 true，已有同名文件会覆盖
     */
    bool copyIntoToolDir(const std::string& srcPath, const std::string& toolDataDir,
                         const std::string& fileName, std::string* err) const;

private:
    /************************************************** 内部 **************************************************/
    /** @brief 示教参数说明：model 缺省 model.onnx，labels 缺省 labels.txt */
    std::vector<ParamField> learnFields() const;
    /** @brief 运行参数说明：输入宽高（像素）、分数阈值、交并比 0~1、最多框数 */
    std::vector<ParamField> runFields() const;
    /** @brief 取文本参数，learn 里有就读 learn，否则读 run */
    std::string textOf(const ToolIn& in, const char* key) const;
    /** @brief 方案未保存或模型、标签文件不在时失败 */
    bool filesReady(const ToolIn& in, std::string* err) const;
    /** @brief 工具数据目录拼文件名 */
    std::string joinToolFile(const ToolIn& in, const std::string& fileName) const;
    /** @brief 路径存在且是文件（UTF-8 路径） */
    bool fileExists(const std::string& path) const;
};

}
#endif
