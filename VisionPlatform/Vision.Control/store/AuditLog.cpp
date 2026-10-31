/**
 * @file AuditLog.cpp
 * @brief 审计日志读写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AuditLog.h"
#include "RemoteFiles.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace Vision {

AuditLog::AuditLog()
    : m_dir(QDir(RemoteFiles::logDir()).filePath("audit"))
    , m_op(RemoteFiles::userName())
{
    QDir().mkpath(m_dir);
}

QString AuditLog::dir() const
{
    return m_dir;
}

QString AuditLog::fileOf(const QDate& day) const
{
    return QDir(m_dir).filePath(day.toString("yyyy-MM-dd") + ".jsonl");
}

void AuditLog::add(const QString& event, const QString& device, const QString& deviceId, const QString& source,
    const QString& detail, qint64 seconds)
{
    const QDateTime now = QDateTime::currentDateTime();
    QJsonObject obj;
    obj.insert("time", now.toString(Qt::ISODate));
    obj.insert("event", event);
    obj.insert("device", device);
    obj.insert("deviceId", deviceId);
    obj.insert("op", m_op);
    obj.insert("source", source);
    obj.insert("detail", detail);
    if (seconds > 0) {
        obj.insert("seconds", seconds);
    }
    QFile file(fileOf(now.date()));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        file.write("\n");
    }
}

std::vector<AuditLog::Entry> AuditLog::query(const QDate& from, const QDate& to) const
{
    std::vector<Entry> out;
    for (QDate day = to; day >= from; day = day.addDays(-1)) {
        QFile file(fileOf(day));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        std::vector<Entry> today;
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            if (line.isEmpty()) {
                continue;
            }
            const QJsonObject obj = QJsonDocument::fromJson(line).object();
            if (obj.isEmpty()) {
                continue;
            }
            Entry entry;
            entry.time = QDateTime::fromString(obj.value("time").toString(), Qt::ISODate);
            entry.event = obj.value("event").toString();
            entry.device = obj.value("device").toString();
            entry.deviceId = obj.value("deviceId").toString();
            entry.op = obj.value("op").toString();
            entry.source = obj.value("source").toString();
            entry.detail = obj.value("detail").toString();
            entry.seconds = static_cast<qint64>(obj.value("seconds").toDouble());
            today.push_back(entry);
        }
        out.insert(out.end(), today.rbegin(), today.rend());
    }
    return out;
}

void AuditLog::prune(int days) const
{
    if (days <= 0) {
        return;
    }
    const QDate keep = QDate::currentDate().addDays(-days);
    const QStringList files = QDir(m_dir).entryList(QStringList() << "*.jsonl", QDir::Files);
    for (int i = 0; i < files.size(); ++i) {
        const QDate day = QDate::fromString(files.at(i).left(10), "yyyy-MM-dd");
        if (day.isValid() && day < keep) {
            QFile::remove(QDir(m_dir).filePath(files.at(i)));
        }
    }
}

QString AuditLog::title(const QString& event)
{
    if (event == "session.start") {
        return QString::fromUtf8("会话开始");
    }
    if (event == "session.end") {
        return QString::fromUtf8("会话结束");
    }
    if (event == "operate.start") {
        return QString::fromUtf8("取得操作");
    }
    if (event == "operate.end") {
        return QString::fromUtf8("释放操作");
    }
    if (event == "operate.reject") {
        return QString::fromUtf8("操作被拒");
    }
    if (event == "config.push") {
        return QString::fromUtf8("修改共享");
    }
    if (event == "screenshot") {
        return QString::fromUtf8("截图");
    }
    if (event == "file.upload") {
        return QString::fromUtf8("上传文件");
    }
    if (event == "file.download") {
        return QString::fromUtf8("下载文件");
    }
    if (event == "power") {
        return QString::fromUtf8("电源");
    }
    if (event == "privacy") {
        return QString::fromUtf8("隐私屏");
    }
    if (event == "record.start" || event == "record.stop") {
        return QString::fromUtf8("录屏");
    }
    if (event == "device.add") {
        return QString::fromUtf8("添加设备");
    }
    if (event == "device.remove") {
        return QString::fromUtf8("删除设备");
    }
    return event;
}

const char* AuditLog::tone(const QString& event)
{
    if (event == "session.start") {
        return "green";
    }
    if (event == "operate.start") {
        return "blue";
    }
    if (event == "operate.reject") {
        return "red";
    }
    if (event == "config.push") {
        return "orange";
    }
    if (event == "screenshot" || event == "privacy") {
        return "purple";
    }
    if (event == "file.upload" || event == "file.download") {
        return "blue";
    }
    if (event == "power" || event == "record.start") {
        return "orange";
    }
    return "gray";
}

QString AuditLog::duration(qint64 seconds)
{
    if (seconds < 60) {
        return QString::fromUtf8("%1 秒").arg(seconds);
    }
    if (seconds < 3600) {
        return QString::fromUtf8("%1 分 %2 秒").arg(seconds / 60).arg(seconds % 60);
    }
    return QString::fromUtf8("%1 小时 %2 分").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

}
