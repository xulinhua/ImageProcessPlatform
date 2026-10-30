/**
 * @file CloudFileGrabber.h
 * @brief 从文件/目录读取 ASCII 点云（PLY / PCD）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLOUDFILEGRABBER_H
#define VISION_CLOUDFILEGRABBER_H

#include "VisionCommonExport.h"
#include "ParamValue.h"
#include <HsCommData/HsCloud.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CloudFileGrabber
 * @brief 点云文件抓取。对齐 FileGrabber：单文件或目录循环。无 Qt、无 PCL
 */
class VISION_COMMON_API CloudFileGrabber {
public:
    /** @brief 目录下的点云文件列表；path 为文件则只含它自己 */
    static std::vector<std::string> cloudFiles(const std::string& path);

    /**
     * @brief 读一份 ASCII PLY 或 PCD
     * @param file 绝对或可直接打开的路径；空则清空 out 并返回 true
     * @param out 输出点云
     * @param err 失败原因
     */
    static bool load(const std::string& file, HsCommData::Cloud3* out, std::string* err);

    /**
     * @brief 读一帧。sourceMode 为 file 时固定该文件，否则按目录循环
     * @param runtime 进出：Index、GrabPath、LastFile
     * @param out 输出点云
     * @param err 失败原因
     */
    static bool grab(const std::string& path, const std::string& sourceMode,
                     ParamMap* runtime, HsCommData::Cloud3* out, std::string* err);

private:
    /** @brief 拼接路径 */
    static std::string joinPath(const std::string& a, const std::string& b);
    /** @brief 扩展名小写 */
    static std::string extLower(const std::string& n);
    /** @brief 是否为支持的点云文件名 */
    static bool isCloudName(const std::string& n);
    /** @brief 路径是否存在 */
    static bool pathExists(const std::string& p);
    /** @brief 是否目录 */
    static bool isDir(const std::string& p);
    /** @brief 去掉首尾空白和换行 */
    static std::string trimLine(const std::string& line);
    /** @brief 按空白拆成浮点 */
    static bool splitDoubles(const std::string& line, std::vector<double>* nums);
    /** @brief 已读过首行 ply 之后的 PLY 正文 */
    static bool readPly(FILE* fp, HsCommData::Cloud3* cloud, std::string* err);
    /** @brief 从文件头开始读 PCD */
    static bool readPcd(FILE* fp, HsCommData::Cloud3* cloud, std::string* err);
};

}

#endif
