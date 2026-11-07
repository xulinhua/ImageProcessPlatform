/**
 * @file session_worker.h
 * @brief 在工作线程里打开通道、发送和 poll
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 界面线程只投递请求、取走已经拷好的记录。poll 填出的指针在下一次 poll 前就失效。
 */

#ifndef HS_IPC_DEBUG_SESSION_WORKER_H
#define HS_IPC_DEBUG_SESSION_WORKER_H

#include "owned_msg.h"

#include <QByteArray>
#include <QMutex>
#include <QString>
#include <QThread>
#include <deque>

/** @brief 表格里的一行。图像像素已经离开库的缓冲 */
struct LogRow {
    QString time;
    QString dir;
    QString seq;
    QString type;
    QString summary;
    QByteArray image;
    bool jpeg;
    LogRow() : jpeg(false) {}
};

/**
 * @brief 唯一持有 HsIpc::Session 的对象。收发都走这个类
 */
class SessionWorker : public QThread {
public:
    SessionWorker();
    ~SessionWorker();

    /** @brief 投递打开。name、host 由本对象拷走 */
    void requestOpen(int transport, const QString& name, const QString& host, int port,
                     int capacity, uint32_t imageSlotBytes);
    /** @brief 投递关闭 */
    void requestClose();
    /** @brief 投递一条已拷贝的报文 */
    void requestSend(const OwnedMsg& msg);
    /** @brief 取走一行记录。没有则返回 false */
    bool takeRow(LogRow* row);
    /** @brief 通道是否已打开 */
    bool opened() const;
    /** @brief 最近一次打开失败的原因 */
    QString lastError() const;
    /** @brief 距上次成功 poll 的毫秒。还没收到过为 -1 */
    int idleMs() const;

protected:
    void run() override;

private:
    enum JobKind { JobOpen, JobClose, JobSend };

    struct Job {
        JobKind kind;
        int transport;
        std::string name;
        std::string host;
        int port;
        int capacity;
        uint32_t imageSlotBytes;
        OwnedMsg msg;
        Job()
            : kind(JobClose)
            , transport(HS_IPC_SHARED_MEMORY)
            , port(0)
            , capacity(32)
            , imageSlotBytes(1024u * 1024u)
        {}
    };

    void pushRow(const LogRow& row);
    void handle(Job* job, HsIpc::Session* session);
    LogRow rowFrom(const char* dir, const HsIpcMessage& msg, const std::vector<uint8_t>& image);

    mutable QMutex m_mu;
    std::deque<Job> m_jobs;
    std::deque<LogRow> m_rows;
    bool m_stop;
    bool m_opened;
    int m_idleMs;
    int m_transport;
    int m_chunk;
    QString m_error;
};

#endif
