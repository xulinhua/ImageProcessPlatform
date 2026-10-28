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

class SolutionIoCheck {
public:
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

    void removeDir()
    {
        if (m_dir.empty()) {
            return;
        }
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path(utf8ToWide(m_dir)), ec);
    }

    std::string filePath(const char* name) const
    {
        return m_dir + name;
    }

    void fail(const char* name, const std::string& detail)
    {
        std::cout << "FAIL " << name;
        if (!detail.empty()) {
            std::cout << ": " << detail;
        }
        std::cout << std::endl;
        ++m_failed;
    }

    static int jobCount(const Solution& s)
    {
        int n = 0;
        for (size_t i = 0; i < s.stations.size(); ++i) {
            n += static_cast<int>(s.stations[i].jobs.size());
        }
        return n;
    }

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

    bool saveLoad(const Solution& in, Solution* out, std::string* error)
    {
        if (!VisionCfgMgr::saveSolution(in, error)) {
            return false;
        }
        return VisionCfgMgr::loadSolution(in.filePath, out, error);
    }

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

    std::string m_dir;
    int m_failed;
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
