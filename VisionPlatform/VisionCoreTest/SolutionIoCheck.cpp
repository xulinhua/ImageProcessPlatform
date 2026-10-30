/**
 * @file SolutionIoCheck.cpp
 * @brief 方案存取自检：在临时目录做空方案/最小方案往返、缺失文件与非法路径加载检查
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobCfg.h"
#include "PortTypes.h"
#include "Solution.h"
#include "StationCfg.h"
#include "TextEncoding.h"
#include "ToolCfg.h"
#include "VisionCfgMgr.h"

#include <filesystem>
#include <iostream>
#include <string>

#include <windows.h>

namespace Vision {

/**
 * @class SolutionIoCheck
 * @brief VisionCfgMgr 方案读写的回归检查；失败项打印到 stdout
 */
class SolutionIoCheck {
public:
    /** @brief 依次跑全部检查，返回失败项个数 */
    int run()
    {
        m_failed = 0;
        prepareDir();
        checkEmptyRoundTrip();
        checkMinimalRoundTrip();
        checkMissingFile();
        checkUnreadablePath();
        removeDir();
        return m_failed;
    }

private:
    /** @brief 在系统临时目录下建 VisionCoreTest-<pid> 工作目录 */
    void prepareDir()
    {
        wchar_t buf[MAX_PATH];
        const DWORD n = GetTempPathW(MAX_PATH, buf);
        std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(buf) : L".\\";
        dir += L"VisionCoreTest-";
        dir += std::to_wstring(GetCurrentProcessId());
        dir += L"\\";
        CreateDirectoryW(dir.c_str(), 0);
        m_dir = wideToUtf8(dir.c_str());
    }

    /** @brief 删除工作目录 */
    void removeDir()
    {
        if (m_dir.empty()) {
            return;
        }
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path(utf8ToWide(m_dir)), ec);
    }

    /** @brief 工作目录下的文件路径（UTF-8） */
    std::string filePath(const char* name) const
    {
        return m_dir + name;
    }

    /** @brief 打印一条失败并计数 */
    void fail(const char* name, const std::string& detail)
    {
        std::cout << "FAIL " << name;
        if (!detail.empty()) {
            std::cout << ": " << detail;
        }
        std::cout << std::endl;
        ++m_failed;
    }

    /** @brief 全部工站的流程总数 */
    static int jobCount(const Solution& s)
    {
        int n = 0;
        for (size_t i = 0; i < s.stations.size(); ++i) {
            n += static_cast<int>(s.stations[i].jobs.size());
        }
        return n;
    }

    /** @brief 全部流程的工具总数 */
    static int toolCount(const Solution& s)
    {
        int n = 0;
        for (size_t i = 0; i < s.stations.size(); ++i) {
            const StationCfg& st = s.stations[i];
            for (size_t j = 0; j < st.jobs.size(); ++j) {
                n += static_cast<int>(st.jobs[j].tools.size());
            }
        }
        return n;
    }

    /** @brief 按 in.filePath 存盘后再读回到 out */
    bool saveLoad(const Solution& in, Solution* out, std::string* error)
    {
        if (!VisionCfgMgr::saveSolution(in, error)) {
            return false;
        }
        return VisionCfgMgr::loadSolution(in.filePath, out, error);
    }

    /** @brief 空方案往返：计数为 0、名字保留 */
    void checkEmptyRoundTrip()
    {
        const char* name = "empty-roundtrip";
        Solution in;
        in.name = "empty";
        in.filePath = filePath("empty.vpsol");
        Solution out;
        std::string error;
        if (!saveLoad(in, &out, &error)) {
            fail(name, error);
            return;
        }
        if (out.stations.size() != 0 || jobCount(out) != 0 || toolCount(out) != 0) {
            fail(name, "counts are not zero");
            return;
        }
        if (out.name != in.name) {
            fail(name, "name mismatch");
        }
    }

    /** @brief 一工站一流程一工具一连线往返：id、连线和端口保留 */
    void checkMinimalRoundTrip()
    {
        const char* name = "minimal-roundtrip";
        const ObjectId stationId = ObjectId::create();
        const ObjectId jobId = ObjectId::create();
        const ObjectId toolId = ObjectId::create();
        const ObjectId upstreamId = ObjectId::create();

        Solution in;
        in.name = "minimal";
        in.filePath = filePath("minimal.vpsol");

        StationCfg station;
        station.id = stationId;
        station.name = "station";

        JobCfg job;
        job.id = jobId;
        job.name = "job";
        job.execMode = ExecCalled;
        job.root.kind = GroupSequence;
        job.root.id = ObjectId::create();

        ToolCfg tool;
        tool.id = toolId;
        tool.name = "tool";
        tool.toolName = "judge";
        tool.type = ToolType::Logic;
        job.tools.push_back(tool);
        ToolLink link;
        link.fromId = upstreamId;
        link.toId = toolId;
        link.fromPort = portSlotOut(PortData::Image);
        link.toPort = portSlotIn(PortData::Image);
        job.links.push_back(link);
        station.jobs.push_back(job);
        in.stations.push_back(station);

        // 存盘读回后逐层核对
        Solution out;
        std::string error;
        if (!saveLoad(in, &out, &error)) {
            fail(name, error);
            return;
        }
        if (out.stations.size() != 1 || jobCount(out) != 1 || toolCount(out) != 1) {
            fail(name, "counts mismatch");
            return;
        }
        const StationCfg& st = out.stations[0];
        const JobCfg& loadedJob = st.jobs[0];
        const ToolCfg& loadedTool = loadedJob.tools[0];
        if (st.id != stationId || loadedJob.id != jobId || loadedTool.id != toolId) {
            fail(name, "id mismatch");
            return;
        }
        if (loadedJob.links.size() != 1 || loadedJob.links[0].fromId != upstreamId) {
            fail(name, "link mismatch");
            return;
        }
        const ToolLink& loaded = loadedJob.links[0];
        if (loaded.fromPort != link.fromPort || loaded.toPort != link.toPort) {
            fail(name, "port mismatch");
        }
    }

    /** @brief 加载不存在的文件必须失败并给出错误 */
    void checkMissingFile()
    {
        const char* name = "missing-file";
        Solution out;
        std::string error;
        const bool ok = VisionCfgMgr::loadSolution(filePath("missing.vpsol"), &out, &error);
        if (ok || error.empty()) {
            fail(name, ok ? "load returned true" : "empty error");
        }
    }

    /** @brief 路径是目录时加载必须失败并给出错误 */
    void checkUnreadablePath()
    {
        const char* name = "unreadable-path";
        const std::string path = filePath("not-a-file");
        CreateDirectoryW(utf8ToWide(path).c_str(), 0);
        Solution out;
        std::string error;
        const bool ok = VisionCfgMgr::loadSolution(path, &out, &error);
        if (ok || error.empty()) {
            fail(name, ok ? "load returned true" : "empty error");
        }
    }

    std::string m_dir; ///< 工作目录（UTF-8，以反斜杠结尾）
    int m_failed;      ///< 失败项计数
};

} // namespace Vision

int main()
{
    Vision::SolutionIoCheck check;
    const int failed = check.run();
    if (failed == 0) {
        std::cout << "VisionCoreTest passed" << std::endl;
        return 0;
    }
    std::cout << "VisionCoreTest failed: " << failed << std::endl;
    return 1;
}
