/**
 * @file StartupSweep.h
 * @brief 启动时结束同名进程，并放开这些进程留下的端口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STARTUP_SWEEP_H
#define VISION_STARTUP_SWEEP_H

#include <string>
#include <vector>

namespace Vision {

/**
 * @class StartupSweep
 * @brief 按角色结束同名进程，再处理仍占着相关端口的连接
 * @note 不结束当前进程。界面启动不结束带 --service / --agent 的进程；代理启动只结束带 --agent 的进程
 */
class StartupSweep {
public:
    /** @brief 和当前进程同一类的同名进程才结束 */
    enum Role {
        RolePlain = 0,  ///< 命令行里没有 --service、--agent
        RoleAgent = 1   ///< 命令行里有 --agent
    };

    /** @brief 这次实际做了什么。没有任何残留时 text 为空 */
    struct Report {
        int killed;         ///< 已结束的同名进程数
        int portsClosed;    ///< 已放开的端口数
        int portsLeft;      ///< 仍被别人占着、没有动的端口数
        std::string text;   ///< 给弹窗和日志的说明，一行一条

        /** @brief 结束过进程，或动过端口，或发现端口被别人占着 */
        bool any() const
        {
            return killed > 0 || portsClosed > 0 || portsLeft > 0;
        }
    };

    /**
     * @brief 结束同角色的同名进程，再清理 tcpPorts / udpPorts 上已退出进程留下的占用
     * @note 别的程序占着的端口只记下来，不结束那个程序
     */
    static Report run(Role role, const std::vector<int>& tcpPorts, const std::vector<int>& udpPorts);

private:
    /** @brief 当前进程的 exe 文件名，不含目录 */
    static std::wstring ourExeName();
    /** @brief 读进程命令行。读不到时返回空 */
    static std::wstring commandLineOf(unsigned long pid);
    /** @brief 命令行是否属于这次要清理的角色 */
    static bool matchesRole(Role role, const std::wstring& cmd);
    /** @brief 进程还在 */
    static bool alive(unsigned long pid);
    /** @brief 结束进程并等到退出。失败返回 false */
    static bool killPid(unsigned long pid);
    /** @brief exe 文件名是否和当前进程相同，忽略大小写 */
    static bool sameName(const wchar_t* exeName);
    /** @brief 进程 exe 文件名。读不到返回空 */
    static std::wstring exeNameOf(unsigned long pid);
    /** @brief 把仍挂在端口上、所属进程已经没了的 TCP 连接删掉 */
    static int releaseDeadTcp(int port);
    /** @brief 追加一行说明 */
    static void addLine(Report* report, const std::string& line);
};

} // namespace Vision

#endif
