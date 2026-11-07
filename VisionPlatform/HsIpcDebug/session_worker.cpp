/**
 * @file session_worker.cpp
 * @brief 工作线程收发
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "session_worker.h"

#include <QDateTime>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

SessionWorker::SessionWorker()
    : m_stop(false)
    , m_opened(false)
    , m_idleMs(-1)
    , m_transport(HS_IPC_SHARED_MEMORY)
    , m_chunk(0)
{
}

SessionWorker::~SessionWorker()
{
    {
        QMutexLocker lock(&m_mu);
        m_stop = true;
    }
    wait();
}

void SessionWorker::requestOpen(int transport, const QString& name, const QString& host, int port,
                                int capacity, uint32_t imageSlotBytes)
{
    Job job;
    job.kind = JobOpen;
    job.transport = transport;
    job.name = name.toUtf8().constData();
    job.host = host.toUtf8().constData();
    job.port = port;
    job.capacity = capacity;
    job.imageSlotBytes = imageSlotBytes;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
    m_error.clear();
}

void SessionWorker::requestClose()
{
    Job job;
    job.kind = JobClose;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

void SessionWorker::requestSend(const OwnedMsg& msg)
{
    Job job;
    job.kind = JobSend;
    job.msg = msg;
    QMutexLocker lock(&m_mu);
    m_jobs.push_back(job);
}

bool SessionWorker::takeRow(LogRow* row)
{
    if (!row) {
        return false;
    }
    QMutexLocker lock(&m_mu);
    if (m_rows.empty()) {
        return false;
    }
    *row = m_rows.front();
    m_rows.pop_front();
    return true;
}

bool SessionWorker::opened() const
{
    QMutexLocker lock(&m_mu);
    return m_opened;
}

QString SessionWorker::lastError() const
{
    QMutexLocker lock(&m_mu);
    return m_error;
}

int SessionWorker::idleMs() const
{
    QMutexLocker lock(&m_mu);
    return m_idleMs;
}

void SessionWorker::pushRow(const LogRow& row)
{
    QMutexLocker lock(&m_mu);
    m_rows.push_back(row);
    while (m_rows.size() > 500) {
        m_rows.pop_front();
    }
}

LogRow SessionWorker::rowFrom(const char* dir, const HsIpcMessage& msg, const std::vector<uint8_t>& image)
{
    LogRow row;
    row.time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    row.dir = QString::fromUtf8(dir);
    row.seq = QString::number(msg.seq);
    row.type = QString::fromUtf8(ipcTypeText(msg.type));
    QString summary = row.type;
    if (msg.name && msg.name[0]) {
        summary += QString::fromUtf8(" ") + QString::fromUtf8(msg.name);
    }
    if (msg.type == HS_IPC_SIGNAL) {
        summary += msg.result == HS_IPC_RESULT_NG ? QString::fromUtf8(" NG") : QString::fromUtf8(" OK");
        if (msg.text && msg.text[0]) {
            summary += QString::fromUtf8(" ") + QString::fromUtf8(msg.text);
        }
    } else if (msg.type == HS_IPC_STATS) {
        summary.clear();
        for (int i = 0; i < msg.statCount; ++i) {
            if (i) {
                summary += QString::fromUtf8(" ");
            }
            const HsIpcStat& st = msg.stats[i];
            summary += QString::fromUtf8(st.key ? st.key : "");
            summary += "=";
            if (st.type == HS_IPC_VALUE_FLOAT) {
                summary += QString::number(st.f64);
            } else {
                summary += QString::number((qlonglong)st.i64);
            }
        }
    } else if (msg.type == HS_IPC_IMAGE || msg.type == HS_IPC_FILE) {
        summary += QString::fromUtf8(" %1 字节").arg(image.size() ? (int)image.size() : (int)msg.dataBytes);
        if ((msg.flags & HS_IPC_FLAG_CONT) || m_chunk > 0) {
            ++m_chunk;
            summary += QString::fromUtf8(" 分片 %1").arg(m_chunk);
            if ((msg.flags & HS_IPC_FLAG_CONT) == 0) {
                m_chunk = 0;
            }
        }
        if (msg.path && msg.path[0]) {
            summary += QString::fromUtf8(" ") + QString::fromUtf8(msg.path);
        }
    }
    row.summary = summary;
    if (msg.type == HS_IPC_IMAGE && !image.empty()) {
        row.image = QByteArray(reinterpret_cast<const char*>(&image[0]), (int)image.size());
        row.jpeg = msg.format == HS_IPC_IMAGE_JPEG;
    }
    return row;
}

void SessionWorker::handle(Job* job, HsIpc::Session* session)
{
    if (job->kind == JobClose) {
        *session = HsIpc::Session();
        QMutexLocker lock(&m_mu);
        m_opened = false;
        return;
    }
    if (job->kind == JobOpen) {
        *session = HsIpc::Session();
        HsIpcConfig cfg;
        std::memset(&cfg, 0, sizeof(cfg));
        cfg.transport = job->transport;
        cfg.name = job->name.c_str();
        cfg.host = job->host.c_str();
        cfg.port = job->port;
        cfg.signalCapacity = job->capacity;
        cfg.imageSlotBytes = job->imageSlotBytes;
        cfg.fileSlotBytes = job->imageSlotBytes;
        std::string err;
        HsIpc::Session opened = HsIpc::Session::open(cfg, &err);
        QMutexLocker lock(&m_mu);
        if (!opened.opened()) {
            m_opened = false;
            m_error = QString::fromUtf8(err.c_str());
            *session = HsIpc::Session();
            return;
        }
        m_opened = true;
        m_error.clear();
        m_transport = job->transport;
        m_idleMs = -1;
        m_chunk = 0;
        lock.unlock();
        *session = std::move(opened);
        return;
    }
    if (!session->opened()) {
        LogRow row;
        row.time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        row.dir = QString::fromUtf8("发送");
        row.type = QString::fromUtf8(ipcTypeText(job->msg.type));
        row.summary = QString::fromUtf8("通道未打开");
        pushRow(row);
        return;
    }
    HsIpcMessage msg = job->msg.toMessage();
    const int rc = session->send(msg);
    LogRow row;
    row.time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    row.dir = QString::fromUtf8("发送");
    row.type = QString::fromUtf8(ipcTypeText(job->msg.type));
    row.summary = QString::fromUtf8(ipcCodeText(rc));
    if ((job->msg.type == HS_IPC_IMAGE || job->msg.type == HS_IPC_FILE)
        && m_transport != HS_IPC_SHARED_MEMORY
        && job->msg.data.size() > HS_IPC_CHUNK_BYTES) {
        const int pieces = (int)((job->msg.data.size() + HS_IPC_CHUNK_BYTES - 1) / HS_IPC_CHUNK_BYTES);
        row.summary += QString::fromUtf8("，拆成 %1 片").arg(pieces);
    }
    if (job->msg.type == HS_IPC_SIGNAL) {
        row.summary += QString::fromUtf8(" ") + QString::fromUtf8(job->msg.name.c_str());
    }
    if (job->msg.type == HS_IPC_IMAGE) {
        row.image = QByteArray(reinterpret_cast<const char*>(job->msg.data.data()), (int)job->msg.data.size());
        row.jpeg = job->msg.format == HS_IPC_IMAGE_JPEG;
    }
    pushRow(row);
}

void SessionWorker::run()
{
    HsIpc::Session session;
    while (true) {
        Job job;
        bool hasJob = false;
        {
            QMutexLocker lock(&m_mu);
            if (m_stop) {
                break;
            }
            if (!m_jobs.empty()) {
                job = m_jobs.front();
                m_jobs.pop_front();
                hasJob = true;
            }
        }
        if (hasJob) {
            handle(&job, &session);
        }
        if (session.opened()) {
            HsIpcMessage got;
            std::memset(&got, 0, sizeof(got));
            const int rc = session.poll(&got, 80);
            {
                QMutexLocker lock(&m_mu);
                const uint64_t last = session.lastRecvMs();
                m_idleMs = last == 0 ? -1 : (int)(GetTickCount64() - last);
            }
            if (rc == HS_IPC_OK) {
                std::vector<uint8_t> image;
                if (got.data && got.dataBytes) {
                    image.assign(got.data, got.data + got.dataBytes);
                }
                pushRow(rowFrom("接收", got, image));
            } else if (rc != HS_IPC_TIMEOUT) {
                LogRow row;
                row.time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
                row.dir = QString::fromUtf8("接收");
                row.summary = QString::fromUtf8(ipcCodeText(rc));
                pushRow(row);
            }
        } else if (!hasJob) {
            msleep(30);
        }
    }
}
