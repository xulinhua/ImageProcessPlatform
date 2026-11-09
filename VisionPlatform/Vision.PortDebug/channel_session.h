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

#include <QByteArray>
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
    /** @brief 为真时写入日志，不写入接收区 */
    bool op;
    /** @brief 报文来源，TCP 服务端和 UDP 使用 */
    QString peer;
    /** @brief Modbus 功能码。0 表示不是寄存器表要填的结果 */
    int fn;
    int address;
    int count;
    /** @brief 空格分隔的寄存器或线圈值 */
    QString values;
    /** @brief 这一帧的原始字节。接收另存为文件时只写它 */
    QByteArray bytes;
    CommLine() : op(false), fn(0), address(0), count(0) {}
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
    void requestModbusBits(int slave, int fn, int address, const std::vector<unsigned char>& bits, const QString& peerId);
    void requestLines(bool dtr, bool rts);
    void requestUdp(const QString& host, int port);
    void requestKick(const QString& peerId);
    void setPeer(const QString& peerId);
    void setGap(int ms);
    /** @brief 接收行按当前勾选格式化。只影响之后收到的数据 */
    void setView(bool hex, bool stamp);

    bool takeLine(CommLine* line);
    bool opened() const;
    QString lastError() const;
    QStringList peers() const;
    /** @brief 成功发出和收到的字节、帧数。界面用来做状态栏计数 */
    void stats(quint64* rxBytes, quint64* txBytes, quint64* rxFrames, quint64* txFrames, quint64* errors = 0) const;
    void resetStats();

protected:
    void run() override;

private:
    enum JobKind { JobOpen, JobClose, JobWrite, JobHttp, JobWs, JobModbus, JobLines, JobUdp, JobKick };

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
        std::vector<unsigned char> bits;
        bool dtr;
        bool rts;
        int udpPort;
        Job()
            : kind(JobClose)
            , stream(true)
            , webSocket(false)
            , text(true)
            , slave(1)
            , fn(3)
            , address(0)
            , count(1)
            , dtr(true)
            , rts(true)
            , udpPort(0)
        {}
    };

    void flushAcc(bool force);
    void takeChunk(const QByteArray& bytes, const QString& peer);
    QString peerText(const QString& id) const;

    void pushLine(const QString& text, const QString& raw = QString(), bool op = false, const QByteArray& bytes = QByteArray(), const QString& peer = QString());
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
    quint64 m_errors;
    bool m_server;
    bool m_udp;
    int m_gap;
    QByteArray m_acc;
    QString m_accPeer;
    qint64 m_accMs;
    QString m_error;
    Vision::ObjectId m_id;
};

#endif
