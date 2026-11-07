/**
 * @file channel_session.cpp
 * @brief 工作线程里的打开、读和写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "channel_session.h"
#include "byte_text.h"
#include "CommMgr.h"
#include "Modbus.h"

#include <QDateTime>

ChannelSession::ChannelSession()
    : m_stop(false)
    , m_opened(false)
    , m_stream(true)
    , m_webSocket(false)
    , m_hex(true)
    , m_stamp(true)
    , m_rxBytes(0)
    , m_txBytes(0)
    , m_rxFrames(0)
    , m_txFrames(0)
    , m_id(Vision::ObjectId::fromString("comm-debug"))
{
}

ChannelSession::~ChannelSession()
{
    {
        QMutexLocker lock(&m_mu);
        m_stop = true;
        m_jobs.push_back(Job());
    }
    wait();
}

void ChannelSession::requestOpen(const Vision::CommChannel& channel, bool stream, bool webSocket)
{
    Job job;
    job.kind = JobOpen;
    job.channel = channel;
    job.stream = stream;
    job.webSocket = webSocket;
    QMutexLocker lock(&m_mu);
    m_error.clear();
    m_jobs.push_back(job);
}

void ChannelSession::requestClose()
{
    Job job;
    job.kind = JobClose;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestWrite(const QByteArray& bytes, const QString& peerId)
{
    Job job;
    job.kind = JobWrite;
    job.bytes = bytes;
    job.peerId = peerId;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestHttp(const QString& method, const QString& path, const QString& contentType,
                                 const QString& body, const QString& headers)
{
    Job job;
    job.kind = JobHttp;
    job.method = method;
    job.path = path;
    job.contentType = contentType;
    job.body = body;
    job.headers = headers;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestWs(const QByteArray& bytes, bool text)
{
    Job job;
    job.kind = JobWs;
    job.bytes = bytes;
    job.text = text;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestModbus(int slave, int fn, int address, int count, const std::vector<uint16_t>& regs, const QString& peerId)
{
    Job job;
    job.kind = JobModbus;
    job.slave = slave;
    job.fn = fn;
    job.address = address;
    job.count = count;
    job.regs = regs;
    job.peerId = peerId;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::setPeer(const QString& peerId)
{
    QMutexLocker lock(&m_mu);
    m_peer = peerId;
}

void ChannelSession::setView(bool hex, bool stamp)
{
    QMutexLocker lock(&m_mu);
    m_hex = hex;
    m_stamp = stamp;
}

bool ChannelSession::takeLine(CommLine* line)
{
    if (!line) {
        return false;
    }
    QMutexLocker lock(&m_mu);
    if (m_lines.empty()) {
        return false;
    }
    *line = m_lines.front();
    m_lines.pop_front();
    return true;
}

bool ChannelSession::opened() const
{
    QMutexLocker lock(&m_mu);
    return m_opened;
}

QString ChannelSession::lastError() const
{
    QMutexLocker lock(&m_mu);
    return m_error;
}

QStringList ChannelSession::peers() const
{
    QMutexLocker lock(&m_mu);
    return m_peers;
}

void ChannelSession::stats(quint64* rxBytes, quint64* txBytes, quint64* rxFrames, quint64* txFrames) const
{
    QMutexLocker lock(&m_mu);
    if (rxBytes) {
        *rxBytes = m_rxBytes;
    }
    if (txBytes) {
        *txBytes = m_txBytes;
    }
    if (rxFrames) {
        *rxFrames = m_rxFrames;
    }
    if (txFrames) {
        *txFrames = m_txFrames;
    }
}

void ChannelSession::resetStats()
{
    QMutexLocker lock(&m_mu);
    m_rxBytes = 0;
    m_txBytes = 0;
    m_rxFrames = 0;
    m_txFrames = 0;
}

void ChannelSession::pushLine(const QString& text, const QString& raw)
{
    CommLine line;
    line.text = text;
    line.raw = raw;
    QMutexLocker lock(&m_mu);
    m_lines.push_back(line);
    while (m_lines.size() > 2000) {
        m_lines.pop_front();
    }
}

void ChannelSession::handle(const Job& job)
{
    if (job.kind == JobClose) {
        Vision::CommMgr::instance().closeChannel(m_id);
        QMutexLocker lock(&m_mu);
        m_opened = false;
        m_peers.clear();
        return;
    }
    if (job.kind == JobOpen) {
        Vision::CommMgr::instance().closeChannel(m_id);
        std::string err;
        Vision::CommChannel ch = job.channel;
        ch.id = m_id;
        const bool ok = Vision::CommMgr::instance().openChannel(ch, &err);
        QMutexLocker lock(&m_mu);
        m_opened = ok;
        m_stream = job.stream;
        m_webSocket = job.webSocket;
        m_error = ok ? QString() : QString::fromUtf8(err.c_str());
        if (!ok) {
            lock.unlock();
            pushLine(ByteText::stamp() + QString::fromUtf8("  打开失败  ") + QString::fromUtf8(err.c_str()));
        }
        return;
    }
    bool opened = false;
    {
        QMutexLocker lock(&m_mu);
        opened = m_opened;
    }
    if (!opened) {
        pushLine(ByteText::stamp() + QString::fromUtf8("  通道未打开"));
        return;
    }
    std::string err;
    const std::string peer = job.peerId.toUtf8().constData();
    if (job.kind == JobWrite) {
        Vision::CommBytes data(job.bytes.begin(), job.bytes.end());
        if (!Vision::CommMgr::instance().writeBytes(m_id, peer, data, &err)) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  发送失败  ") + QString::fromUtf8(err.c_str()));
            return;
        }
        QMutexLocker lock(&m_mu);
        m_txBytes += data.size();
        m_txFrames += 1;
        return;
    }
    if (job.kind == JobHttp) {
        int status = 0;
        std::string response;
        const bool ok = Vision::CommMgr::instance().httpExchange(
            m_id, job.method.toUtf8().constData(), job.path.toUtf8().constData(),
            job.contentType.toUtf8().constData(), job.body.toUtf8().constData(),
            &status, &response, &err, job.headers.toUtf8().constData());
        if (!ok) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  HTTP 失败  ") + QString::fromUtf8(err.c_str()));
            return;
        }
        pushLine(ByteText::stamp() + QString::fromUtf8("  HTTP %1").arg(status));
        pushLine(QString::fromUtf8(response.c_str()));
        return;
    }
    if (job.kind == JobWs) {
        Vision::CommBytes data(job.bytes.begin(), job.bytes.end());
        if (!Vision::CommMgr::instance().wsSend(m_id, data, job.text, &err)) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  发送失败  ") + QString::fromUtf8(err.c_str()));
            return;
        }
        QMutexLocker lock(&m_mu);
        m_txBytes += data.size();
        m_txFrames += 1;
        return;
    }
    if (job.kind == JobModbus) {
        std::vector<uint16_t> regs;
        Vision::CommBytes sent;
        Vision::CommBytes received;
        bool ok = false;
        if (job.fn == 3 || job.fn == 4) {
            const Vision::ModbusTable table = job.fn == 4 ? Vision::ModbusInput : Vision::ModbusHolding;
            ok = Vision::CommMgr::instance().modbusReadRegs(m_id, peer, job.slave, table, job.address, job.count, &regs, &err, &sent, &received);
        } else {
            ok = Vision::CommMgr::instance().modbusWriteRegs(m_id, peer, job.slave, Vision::ModbusHolding, job.address, job.regs, &err, &sent, &received);
            regs = job.regs;
        }
        QString wire;
        if (!sent.empty() || !received.empty()) {
            QByteArray out(reinterpret_cast<const char*>(sent.empty() ? 0 : &sent[0]), (int)sent.size());
            QByteArray in(reinterpret_cast<const char*>(received.empty() ? 0 : &received[0]), (int)received.size());
            wire = QString::fromUtf8("发 ") + ByteText::toHex(out) + QString::fromUtf8("\n收 ") + ByteText::toHex(in);
        }
        {
            QMutexLocker lock(&m_mu);
            m_txBytes += sent.size();
            m_rxBytes += received.size();
            if (!sent.empty()) {
                m_txFrames += 1;
            }
            if (!received.empty()) {
                m_rxFrames += 1;
            }
        }
        if (!ok) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  Modbus 失败  ") + QString::fromUtf8(err.c_str()), wire);
            return;
        }
        QString line = ByteText::stamp() + QString::fromUtf8("  寄存器");
        for (size_t i = 0; i < regs.size(); ++i) {
            line += QString(" %1").arg(regs[i]);
        }
        if (regs.empty()) {
            line += QString::fromUtf8(" 已写");
        }
        pushLine(line, wire);
    }
}

void ChannelSession::run()
{
    int tick = 0;
    while (true) {
        Job job;
        bool hasJob = false;
        bool stop = false;
        bool opened = false;
        bool stream = false;
        bool webSocket = false;
        bool hex = true;
        bool stamp = true;
        QString peer;
        {
            QMutexLocker lock(&m_mu);
            stop = m_stop && m_jobs.empty();
            if (!m_jobs.empty()) {
                job = m_jobs.front();
                m_jobs.pop_front();
                hasJob = true;
            }
            opened = m_opened;
            stream = m_stream;
            webSocket = m_webSocket;
            hex = m_hex;
            stamp = m_stamp;
            peer = m_peer;
        }
        if (stop) {
            Vision::CommMgr::instance().closeChannel(m_id);
            break;
        }
        if (hasJob) {
            handle(job);
        }
        if (opened && stream) {
            std::string err;
            Vision::CommBytes got = Vision::CommMgr::instance().readBytes(
                m_id, peer.toUtf8().constData(), 4096, 80, &err);
            if (!got.empty()) {
                QByteArray bytes(reinterpret_cast<const char*>(&got[0]), (int)got.size());
                QString line = stamp ? ByteText::stamp() + QString::fromUtf8("  ") : QString();
                line += QString::fromUtf8("收← ");
                line += hex ? ByteText::toHex(bytes) : QString::fromUtf8(bytes);
                {
                    QMutexLocker lock(&m_mu);
                    m_rxBytes += (quint64)bytes.size();
                    m_rxFrames += 1;
                }
                pushLine(line);
            }
        } else if (opened && webSocket) {
            std::string err;
            bool text = false;
            Vision::CommBytes got = Vision::CommMgr::instance().wsRecv(m_id, 80, &text, &err);
            if (!got.empty()) {
                QByteArray bytes(reinterpret_cast<const char*>(&got[0]), (int)got.size());
                {
                    QMutexLocker lock(&m_mu);
                    m_rxBytes += (quint64)bytes.size();
                    m_rxFrames += 1;
                }
                if (text) {
                    pushLine(ByteText::stamp() + QString::fromUtf8("  收← ") + QString::fromUtf8(bytes));
                } else {
                    pushLine(ByteText::stamp() + QString::fromUtf8("  收← ") + ByteText::toHex(bytes));
                }
            }
        } else if (!hasJob) {
            msleep(30);
        }
        if (++tick >= 8) {
            tick = 0;
            if (opened) {
                const std::vector<Vision::CommPeer> list = Vision::CommMgr::instance().peers(m_id);
                QStringList names;
                for (size_t i = 0; i < list.size(); ++i) {
                    const QString id = QString::fromUtf8(list[i].id.c_str());
                    const QString remote = QString::fromUtf8(list[i].remote.c_str());
                    names << (remote.isEmpty() ? id : remote + "|" + id);
                }
                QMutexLocker lock(&m_mu);
                m_peers = names;
            }
        }
    }
}
