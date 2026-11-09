/**
 * @file solution_procs.h
 * @brief 查找并结束本解决方案目录下启动的进程
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CLEAN_SOLUTION_PROCS_H
#define VISION_CLEAN_SOLUTION_PROCS_H

#include <string>
#include <vector>

/** @brief 可执行文件位于解决方案目录下的一个进程 */
struct SolutionProc {
    unsigned long pid;
    std::wstring name;
    std::wstring path;
};

/**
 * @brief 按写死的进程名查找。客户端机器上没有解决方案和源码
 *
 * 名单是本平台会单独启动的程序，不含本清理工具。插件是 DLL，不会单独成进程。
 */
class SolutionProcs {
public:
    SolutionProcs();

    const std::vector<SolutionProc>& list() const { return m_list; }

    /** @brief 重新枚举。本进程不列入 */
    void refresh();
    /** @brief 结束这些进程。返回没能结束的个数 */
    int kill(const std::vector<unsigned long>& pids, std::wstring* err) const;

private:
    bool named(const std::wstring& fileName) const;

    std::vector<SolutionProc> m_list;
    unsigned long m_self;
};

#endif
