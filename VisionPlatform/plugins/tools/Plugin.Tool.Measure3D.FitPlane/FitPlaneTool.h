/**
 * @file FitPlaneTool.h
 * @brief 拟合平面工具壳
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_FITPLANETOOL_H
#define VISION_FITPLANETOOL_H

#include "ToolBase.h"
#include <HsCommData/HsCloud.h>
#include <string>

namespace Vision {

/**
 * @class FitPlaneTool
 * @brief 拟合平面。点云来自上游，没有上游时读 ASCII PLY 或 PCD
 */
class FitPlaneTool : public ToolBase {
public:
    /************************************************** 身份 **************************************************/
    /** @brief 注册名 measure3d.fit_plane */
    std::string toolName() const;
    /** @brief 工具大类：3D 测量 */
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
    /** @brief 参数说明，同 fields() */
    std::vector<ParamField> paramFields() const;
    /** @brief 沿用基类端口，输出点云口改名 inlier */
    std::vector<PortDesc> ports() const;

    /************************************************** 执行 **************************************************/
    /** @brief 没有上游点云时先读文件，再交 PCL 算法插件拟合 */
    ToolOut execute(const ToolIn& in);
    /** @brief 克隆本实例（本 DLL 内 new） */
    ToolBase* clone() const;

private:
    /************************************************** 参数 **************************************************/
    /** @brief 参数表：file 点云文件，distance 内点距离阈值（点云单位） */
    std::vector<ParamField> fields() const;
    /** @brief 由参数表生成缺省值 */
    ParamMap defaults() const;

    /************************************************** 读点云 **************************************************/
    /** @brief 相对方案目录的文件解析成绝对路径 */
    std::string resolvePath(const ToolIn& in, const std::string& file, std::string* err) const;
    /** @brief 读 ASCII PLY 或 PCD。文件为空时点云保持空 */
    bool loadFileCloud(const ToolIn& in, HsCommData::Cloud3* cloud, std::string* err) const;
};

}

#endif
