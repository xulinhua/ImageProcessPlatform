/**
 * @file channel_session.h
 * @brief 在工作线程里调用 CommMgr
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 界面只投递打开、发送和 Modbus 请求。收到的字节先拷进队列，再由定时器刷到接收框。
 */

#ifndef HS_COMM_DEBUG_CHANNEL_SESSION_H
#define HS_COMM_DEBUG_CHANNEL_SESSION_H

#include "CommTypes.h"

#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThread>
#include <deque>
#include <vector>

/** @brief 一次收发要记到屏幕上的一行 */
struct CommLine {
    QString text;
    /** @brief Modbus 整帧的 HEX。空表示这一行不是原始字节 */
    QString raw;
};

/**
 * @brief 唯一调用 CommMgr 的对象
 */
class ChannelSession : public QThread {
public:
    ChannelSession();
    ~ChannelSession();

    void requestOpen(const Vision::CommChannel& channel, bool stream, bool webSocket);
    void requestClose();
    void requestWrite(const QByteArray& bytes, const QString& peerId);
    void requestHttp(const QString& method, const QString& path, const QString& contentType,
                     const QString& body, const QString& headers);
    void requestWs(const QByteArray& bytes, bool text);
    void requestModbus(int slave, int fn, int address, int count, const std::vector<uint16_t>& regs, const QString& peerId);
    void setPeer(const QString& peerId);
    /** @brief 接收行按当前勾选格式化。只影响之后收到的数据 */
    void setView(bool hex, bool stamp);

    bool takeLine(CommLine* line);
    bool opened() const;
    QString lastError() const;
    QStringList peers() const;
    /** @brief 成功发出和收到的字节、帧数。界面用来做状态栏计数 */
    void stats(quint64* rxBytes, quint64* txBytes, quint64* rxFrames, quint64* txFrames) const;
    void resetStats();

protected:
    void run() override;

private:
    enum JobKind { JobOpen, JobClose, JobWrite, JobHttp, JobWs, JobModbus };

    struct Job {
        JobKind kind;
        Vision::CommChannel channel;
        bool stream;
        bool webSocket;
        QByteArray bytes;
        bool text;
        QString method;
        QString path;
        QString contentType;
        QString body;
        QString headers;
        QString peerId;
        int slave;
        int fn;
        int address;
        int count;
        std::vector<uint16_t> regs;
        Job()
            : kind(JobClose)
            , stream(true)
            , webSocket(false)
            , text(true)
            , slave(1)
            , fn(3)
            , address(0)
            , count(1)
        {}
    };

    void pushLine(const QString& text, const QString& raw = QString());
    void handle(const Job& job);

    mutable QMutex m_mu;
    std::deque<Job> m_jobs;
    std::deque<CommLine> m_lines;
    QStringList m_peers;
    QString m_peer;
    bool m_stop;
    bool m_opened;
    bool m_stream;
    bool m_webSocket;
    bool m_hex;
    bool m_stamp;
    quint64 m_rxBytes;
    quint64 m_txBytes;
    quint64 m_rxFrames;
    quint64 m_txFrames;
    QString m_error;
    Vision::ObjectId m_id;
};

#endif
