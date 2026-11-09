/**
 * @file RemoteLog.h
 * @brief 中控台 / 被控端共用运行日志：内存环、可选落盘、按小时切分
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_LOG_H
#define VISION_REMOTE_LOG_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace Vision {

/**
 * @class RemoteLog
 * @brief 线程安全诊断日志。默认只写内存；写盘默认关，设置里打开后按滚动间隔切文件
 * @note main 里 RemoteCrash::install 之后调 install(role)。落盘策略用 applyDisk / set*
 */
class RemoteLog {
public:
    /** @brief 级别 */
    enum Level {
        Info = 0,
        Warn = 1,
        Error = 2
    };

    /** @brief 一行日志，给窗口批量拉取 */
    struct Line {
        uint64_t seq;       ///< 自增序号，从 1 起
        Level level;        ///< 级别
        char time[24];      ///< HH:MM:SS.mmm
        char tag[24];       ///< 分类，如 画面 / 发送 / 接收 / 解码
        char text[768];     ///< 正文 UTF-8
    };

    /**
     * @brief 设角色；不打开文件（写盘默认关）
     * @param role master / client 等
     */
    static void install(const char* role);

    /**
     * @brief 应用落盘策略（设置加载或应用后调用）
     * @param diskEnabled 是否写盘，默认关
     * @param pictureDisk 画面/发送/接收/解码 的 Info 是否落盘
     * @param rotateMinutes 滚动间隔分钟，建议 15/30/60/120，最小 15
     * @param keepDays 保留天数，<=0 不清理
     */
    static void applyDisk(bool diskEnabled, bool pictureDisk, int rotateMinutes, int keepDays);

    /** @brief 单调毫秒时间戳，用于算步骤差 */
    static uint64_t tickMs();

    /**
     * @brief 格式化写一行。tag / fmt 为 UTF-8；Info 画面链路约每秒最多 12 条
     */
    static void writef(Level level, const char* tag, const char* fmt, ...);

    /** @brief 同 writef，va_list 版 */
    static void vwritef(Level level, const char* tag, const char* fmt, va_list args);

    /**
     * @brief 取出 seq 之后的新行
     * @param seq 入参为已读最大序号，出参更新为本次最后一条；空环时保持原值
     * @param out 缓冲区
     * @param cap out 容量
     * @return 写入条数
     */
    static size_t pull(uint64_t* seq, Line* out, size_t cap);

    /** @brief 清空内存环（文件保留） */
    static void clear();

    /** @brief 当前日志文件路径 UTF-8；未开写盘时为日志目录 */
    static const char* filePath();

    /** @brief 是否正在写盘 */
    static bool diskEnabled();

private:
    RemoteLog();
    static RemoteLog& self();

    uint64_t nowMs() const;
    void stamp(char* buf, size_t cap) const;
    void pushLocked(Level level, const char* tag, const char* text);
    bool throttleInfoLocked(const char* tag);
    bool isPictureTag(const char* tag) const;
    void closeFileLocked();
    void openBucketLocked();
    void purgeOldLocked();
    std::wstring bucketNameLocked() const;

    std::mutex m_mu;
    std::deque<Line> m_lines;
    uint64_t m_nextSeq;
    std::string m_role;
    std::string m_path;
    std::string m_dirUtf8;
    FILE* m_file;
    std::wstring m_openName;
    mutable long long m_freq;
    uint64_t m_bucketMs;
    int m_bucketCount;
    uint64_t m_lastFlushMs;
    bool m_disk;
    bool m_pictureDisk;
    int m_rotateMinutes;
    int m_keepDays;
};

}

#endif
