/**
 * @file Acquire3DTool.h
 * @brief 三维采集：从点云文件或文件夹取帧
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ACQUIRE3DTOOL_H
#define VISION_ACQUIRE3DTOOL_H

#include "ToolBase.h"
#include "ToolParaAcquire3D.hpp"

namespace Vision {

/**
 * @class Acquire3DTool
 * @brief 加载单个点云文件或文件夹循环，输出 Cloud。对齐二维文件采集
 */
class Acquire3DTool : public ToolBase {
public:
    /** @brief 注册名 acquire3d.load_cloud */
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
    /** @brief 只出点云口 */
    std::vector<PortDesc> ports() const;
    /** @brief 按文件或文件夹取点云 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;
    /** @brief 本工具参数对象 */
    const ToolParaBase* toolParam() const;

private:
    /** @brief 相对方案目录解析成可打开路径 */
    std::string resolvePath(const ToolIn& in, const std::string& file) const;

    ToolParaAcquire3D m_param; ///< 参数快照
};

}

#endif
