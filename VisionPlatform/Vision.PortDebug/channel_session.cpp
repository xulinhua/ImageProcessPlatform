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
    , m_errors(0)
    , m_server(false)
    , m_udp(false)
    , m_gap(20)
    , m_accMs(0)
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

void ChannelSession::requestModbusBits(int slave, int fn, int address, const std::vector<unsigned char>& bits, const QString& peerId)
{
    Job job;
    job.kind = JobModbus;
    job.slave = slave;
    job.fn = fn;
    job.address = address;
    job.bits = bits;
    job.peerId = peerId;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestLines(bool dtr, bool rts)
{
    Job job;
    job.kind = JobLines;
    job.dtr = dtr;
    job.rts = rts;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestUdp(const QString& host, int port)
{
    Job job;
    job.kind = JobUdp;
    job.peerId = host;
    job.udpPort = port;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::requestKick(const QString& peerId)
{
    Job job;
    job.kind = JobKick;
    job.peerId = peerId;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void ChannelSession::setGap(int ms)
{
    QMutexLocker lock(&m_mu);
    m_gap = ms < 1 ? 1 : (ms > 1000 ? 1000 : ms);
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

void ChannelSession::stats(quint64* rxBytes, quint64* txBytes, quint64* rxFrames, quint64* txFrames, quint64* errors) const
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
    if (errors) {
        *errors = m_errors;
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

void ChannelSession::pushLine(const QString& text, const QString& raw, bool op, const QByteArray& bytes, const QString& peer)
{
    CommLine line;
    line.text = text;
    line.raw = raw;
    line.op = op;
    line.bytes = bytes;
    line.peer = peer;
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
        m_server = job.channel.type == Vision::CommTcpServer;
        m_udp = job.channel.type == Vision::CommUdp;
        m_acc.clear();
        m_error = ok ? QString() : QString::fromUtf8(err.c_str());
        if (!ok) {
            lock.unlock();
            pushLine(ByteText::stamp() + QString::fromUtf8("  打开失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
        }
        return;
    }
    bool opened = false;
    {
        QMutexLocker lock(&m_mu);
        opened = m_opened;
    }
    if (!opened) {
        pushLine(ByteText::stamp() + QString::fromUtf8("  通道未打开"), QString(), true);
        return;
    }
    std::string err;
    const std::string peer = job.peerId.toUtf8().constData();
    if (job.kind == JobWrite) {
        Vision::CommBytes data(job.bytes.begin(), job.bytes.end());
        if (!Vision::CommMgr::instance().writeBytes(m_id, peer, data, &err)) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  发送失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
            QMutexLocker lock(&m_mu);
            m_errors += 1;
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
        const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        const bool ok = Vision::CommMgr::instance().httpExchange(
            m_id, job.method.toUtf8().constData(), job.path.toUtf8().constData(),
            job.contentType.toUtf8().constData(), job.body.toUtf8().constData(),
            &status, &response, &err, job.headers.toUtf8().constData());
        const qint64 ms = QDateTime::currentMSecsSinceEpoch() - t0;
        if (!ok) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  HTTP 失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
            QMutexLocker lock(&m_mu);
            m_errors += 1;
            return;
        }
        CommLine head;
        head.text = QString::fromUtf8("%1  %2 ms").arg(status).arg(ms);
        head.fn = 0;
        pushLine(head.text);
        pushLine(QString::fromUtf8(response.c_str()));
        pushLine(ByteText::stamp() + "  " + job.method + "  " + head.text, QString(), true);
        return;
    }
    if (job.kind == JobLines) {
        if (!Vision::CommMgr::instance().setSerialLines(m_id, job.dtr, job.rts, &err)) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  串口线电平失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
        }
        return;
    }
    if (job.kind == JobUdp) {
        if (!Vision::CommMgr::instance().setUdpRemote(m_id, job.peerId.toUtf8().constData(), job.udpPort, &err)) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  对端地址失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
        }
        return;
    }
    if (job.kind == JobKick) {
        Vision::CommMgr::instance().closePeer(m_id, job.peerId.toUtf8().constData());
        pushLine(ByteText::stamp() + QString::fromUtf8("  已断开 ") + job.peerId, QString(), true);
        return;
    }
    if (job.kind == JobWs) {
        Vision::CommBytes data(job.bytes.begin(), job.bytes.end());
        if (!Vision::CommMgr::instance().wsSend(m_id, data, job.text, &err)) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  发送失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
            QMutexLocker lock(&m_mu);
            m_errors += 1;
            return;
        }
        QMutexLocker lock(&m_mu);
        m_txBytes += data.size();
        m_txFrames += 1;
        return;
    }
    if (job.kind == JobModbus) {
        std::vector<uint16_t> regs;
        std::vector<unsigned char> bits;
        Vision::CommBytes sent;
        Vision::CommBytes received;
        bool ok = false;
        if (job.fn == 1 || job.fn == 2) {
            const Vision::ModbusTable table = job.fn == 2 ? Vision::ModbusDiscrete : Vision::ModbusCoil;
            ok = Vision::CommMgr::instance().modbusReadBits(m_id, peer, job.slave, table, job.address, job.count, &bits, &err, &sent, &received);
        } else if (job.fn == 5 || job.fn == 15) {
            ok = Vision::CommMgr::instance().modbusWriteBits(m_id, peer, job.slave, Vision::ModbusCoil, job.address, job.bits, &err, &sent, &received);
            bits = job.bits;
        } else if (job.fn == 3 || job.fn == 4) {
            const Vision::ModbusTable table = job.fn == 4 ? Vision::ModbusInput : Vision::ModbusHolding;
            ok = Vision::CommMgr::instance().modbusReadRegs(m_id, peer, job.slave, table, job.address, job.count, &regs, &err, &sent, &received);
        } else {
            ok = Vision::CommMgr::instance().modbusWriteRegs(m_id, peer, job.slave, Vision::ModbusHolding, job.address, job.regs, &err, &sent, &received);
            regs = job.regs;
        }
        QByteArray out(reinterpret_cast<const char*>(sent.empty() ? 0 : &sent[0]), (int)sent.size());
        QByteArray in(reinterpret_cast<const char*>(received.empty() ? 0 : &received[0]), (int)received.size());
        QString wire = QString::fromUtf8("发→ ") + ByteText::toHex(out);
        if (ok) {
            wire += QString::fromUtf8("\n收← ") + ByteText::toHex(in);
        } else {
            wire += QString::fromUtf8("\n收← ") + QString::fromUtf8(err.c_str());
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
            if (!ok) {
                m_errors += 1;
            }
        }
        if (!ok) {
            pushLine(ByteText::stamp() + QString::fromUtf8("  Modbus 失败  ") + QString::fromUtf8(err.c_str()), QString(), true);
            pushLine(wire);
            return;
        }
        CommLine line;
        line.raw = wire;
        line.fn = job.fn;
        line.address = job.address;
        line.count = job.count;
        if (!regs.empty()) {
            for (size_t i = 0; i < regs.size(); ++i) {
                if (i) {
                    line.values += ' ';
                }
                line.values += QString::number(regs[i]);
            }
        } else {
            for (size_t i = 0; i < bits.size(); ++i) {
                if (i) {
                    line.values += ' ';
                }
                line.values += QString::number((int)bits[i]);
            }
        }
        line.text = line.raw;
        pushLine(line.text, line.raw);
        CommLine tagged = line;
        tagged.op = false;
        {
            QMutexLocker lock(&m_mu);
            if (!m_lines.empty()) {
                m_lines.back().fn = line.fn;
                m_lines.back().address = line.address;
                m_lines.back().count = line.count;
                m_lines.back().values = line.values;
            }
        }
    }
}

QString ChannelSession::peerText(const QString& id) const
{
    const QStringList list = peers();
    for (int i = 0; i < list.size(); ++i) {
        const int bar = list[i].lastIndexOf('|');
        if (bar >= 0 && list[i].mid(bar + 1) == id) {
            return list[i].left(bar);
        }
    }
    return id;
}

void ChannelSession::takeChunk(const QByteArray& bytes, const QString& peer)
{
    bool split = false;
    {
        QMutexLocker lock(&m_mu);
        split = !m_acc.isEmpty() && peer != m_accPeer;
    }
    if (split) {
        flushAcc(true);
    }
    QMutexLocker lock(&m_mu);
    m_acc += bytes;
    m_accPeer = peer;
    m_accMs = QDateTime::currentMSecsSinceEpoch();
    m_rxBytes += (quint64)bytes.size();
}

void ChannelSession::flushAcc(bool force)
{
    QByteArray bytes;
    QString peer;
    bool hex = true;
    bool stamp = true;
    {
        QMutexLocker lock(&m_mu);
        if (m_acc.isEmpty()) {
            return;
        }
        if (!force && QDateTime::currentMSecsSinceEpoch() - m_accMs < m_gap) {
            return;
        }
        bytes = m_acc;
        peer = m_accPeer;
        hex = m_hex;
        stamp = m_stamp;
        m_acc.clear();
        m_rxFrames += 1;
    }
    QString line = stamp ? ByteText::stamp() + QString::fromUtf8("  ") : QString();
    line += QString::fromUtf8("收← ");
    if (!peer.isEmpty()) {
        line += peer + QString::fromUtf8("  ");
    }
    line += hex ? ByteText::toHex(bytes) : QString::fromUtf8(bytes);
    pushLine(line, QString(), false, bytes, peer);
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
            bool server = false;
            bool udp = false;
            {
                QMutexLocker lock(&m_mu);
                server = m_server;
                udp = m_udp;
            }
            if (server) {
                const QString id = QString::fromUtf8(Vision::CommMgr::instance().waitIncoming(m_id, 15).c_str());
                if (!id.isEmpty()) {
                    Vision::CommBytes got = Vision::CommMgr::instance().readBytes(m_id, id.toUtf8().constData(), 4096, 0, &err);
                    if (!got.empty()) {
                        takeChunk(QByteArray(reinterpret_cast<const char*>(&got[0]), (int)got.size()), peerText(id));
                    }
                }
            } else {
                Vision::CommBytes got = Vision::CommMgr::instance().readBytes(m_id, peer.toUtf8().constData(), 4096, 15, &err);
                if (!got.empty()) {
                    QString from;
                    if (udp) {
                        from = QString::fromUtf8(Vision::CommMgr::instance().udpLastSource(m_id).c_str());
                    }
                    takeChunk(QByteArray(reinterpret_cast<const char*>(&got[0]), (int)got.size()), from);
                }
            }
            flushAcc(false);
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
                const QString kind = text ? QString::fromUtf8("文本") : QString::fromUtf8("二进制");
                QString line = ByteText::stamp() + QString::fromUtf8("  收← ") + kind
                    + QString::fromUtf8(" %1 字节 ").arg(bytes.size());
                line += text ? QString::fromUtf8(bytes) : ByteText::toHex(bytes);
                pushLine(line);
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
