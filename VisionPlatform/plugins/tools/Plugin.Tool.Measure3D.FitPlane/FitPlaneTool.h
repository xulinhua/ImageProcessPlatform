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
#include <stdio.h>

namespace Vision {

/**
 * @class FitPlaneTool
 * @brief 拟合平面。点云来自上游，没有上游时读 ASCII PLY 或 PCD
 */
class FitPlaneTool : public ToolBase {
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

private:
    /** @brief 参数表 */
    std::vector<ParamField> fields() const;
    /** @brief 由参数表生成缺省值 */
    ParamMap defaults() const;
    /** @brief 相对方案目录的文件解析成绝对路径 */
    std::string resolvePath(const ToolIn& in, const std::string& file, std::string* err) const;
    /** @brief 读 ASCII PLY 或 PCD。文件为空时点云保持空 */
    bool loadFileCloud(const ToolIn& in, HsCommData::Cloud3* cloud, std::string* err) const;
    /** @brief 已读过首行 ply 之后的 PLY 正文 */
    bool readPly(FILE* fp, HsCommData::Cloud3* cloud, std::string* err) const;
    /** @brief 从文件头开始读 PCD */
    bool readPcd(FILE* fp, HsCommData::Cloud3* cloud, std::string* err) const;
    /** @brief 去掉首尾空白和换行 */
    std::string trimLine(const std::string& line) const;
    /** @brief 按空白拆成浮点。失败返回 false */
    bool splitDoubles(const std::string& line, std::vector<double>* nums) const;
};

}

#endif
