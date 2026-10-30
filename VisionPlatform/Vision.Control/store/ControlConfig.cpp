/**
 * @file ControlConfig.cpp
 * @brief control.json 读写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ControlConfig.h"
#include "Messages.h"
#include "RemoteFiles.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>

namespace Vision {

ControlConfig::ControlConfig()
    : host("0.0.0.0")
    , port(RemoteConst::kPort)
    , trayOnClose(true)
    , startHidden(false)
    , autoStart(false)
    , discovery(true)
    , fitDefault(true)
    , shotDir(QDir(RemoteFiles::appDataDir("RemoteMaster")).filePath("screenshots"))
    , autoRecord(false)
    , recordDays(90)
    , auditDays(180)
    , tourSeconds(10)
    , relayOn(false)
    , relayPort(19200)
    , rows(3)
    , cols(3)
    , defaultFps(RemoteConst::kMaxFpsDefault)
    , defaultQuality(1)
    , streamCodec(0)
    , adaptive(true)
    , m_path(RemoteFiles::exeFile("control.json"))
{
}

void ControlConfig::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    host = obj.value("host").toString(host);
    port = obj.value("port").toInt(port);
    trayOnClose = obj.value("trayOnClose").toBool(trayOnClose);
    startHidden = obj.value("startHidden").toBool(startHidden);
    autoStart = obj.value("autoStart").toBool(autoStart);
    discovery = obj.value("discovery").toBool(discovery);
    fitDefault = obj.value("fitDefault").toBool(fitDefault);
    shotDir = obj.value("shotDir").toString(shotDir);
    recordDir = obj.value("recordDir").toString(recordDir);
    autoRecord = obj.value("autoRecord").toBool(autoRecord);
    recordDays = obj.value("recordDays").toInt(recordDays);
    auditDays = obj.value("auditDays").toInt(auditDays);
    tourSeconds = obj.value("tourSeconds").toInt(tourSeconds);
    relayOn = obj.value("relayOn").toBool(relayOn);
    relayHost = obj.value("relayHost").toString(relayHost);
    relayPort = obj.value("relayPort").toInt(relayPort);
    relayKey = obj.value("relayKey").toString(relayKey);
    rows = obj.value("rows").toInt(rows);
    cols = obj.value("cols").toInt(cols);
    defaultFps = obj.value("defaultFps").toInt(defaultFps);
    defaultQuality = obj.value("defaultQuality").toInt(defaultQuality);
    streamCodec = obj.value("streamCodec").toInt(streamCodec);
    adaptive = obj.value("adaptive").toBool(adaptive);
    if (defaultFps < RemoteConst::kMaxFpsMin) {
        defaultFps = RemoteConst::kMaxFpsMin;
    }
    if (defaultFps > RemoteConst::kMaxFpsMax) {
        defaultFps = RemoteConst::kMaxFpsMax;
    }
    if (defaultQuality < 0 || defaultQuality > 2) {
        defaultQuality = 1;
    }
    if (streamCodec < 0 || streamCodec > 3) {
        streamCodec = 0;
    }
}

void ControlConfig::save() const
{
    QJsonObject obj;
    obj.insert("host", host);
    obj.insert("port", port);
    obj.insert("trayOnClose", trayOnClose);
    obj.insert("startHidden", startHidden);
    obj.insert("autoStart", autoStart);
    obj.insert("discovery", discovery);
    obj.insert("fitDefault", fitDefault);
    obj.insert("shotDir", shotDir);
    obj.insert("recordDir", recordDir);
    obj.insert("autoRecord", autoRecord);
    obj.insert("recordDays", recordDays);
    obj.insert("auditDays", auditDays);
    obj.insert("tourSeconds", tourSeconds);
    obj.insert("relayOn", relayOn);
    obj.insert("relayHost", relayHost);
    obj.insert("relayPort", relayPort);
    obj.insert("relayKey", relayKey);
    obj.insert("rows", rows);
    obj.insert("cols", cols);
    obj.insert("defaultFps", defaultFps);
    obj.insert("defaultQuality", defaultQuality);
    obj.insert("streamCodec", streamCodec);
    obj.insert("adaptive", adaptive);
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
        file.commit();
    }
}

void ControlConfig::purgeRecords() const
{
    const QString dirPath = recordDir.isEmpty() ? shotDir : recordDir;
    if (recordDays <= 0 || dirPath.isEmpty()) {
        return;
    }
    QDir dir(dirPath);
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-recordDays);
    const QStringList names = dir.entryList(QStringList() << "*.mp4" << "*.avi", QDir::Files);
    for (int i = 0; i < names.size(); ++i) {
        const QFileInfo info(dir.filePath(names.at(i)));
        if (info.lastModified() < cutoff) {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

void ControlConfig::applyAutoStart() const
{
    QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (autoStart) {
        run.setValue("RemoteMaster", QString("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
        run.remove("Vision.Control");
    } else {
        run.remove("RemoteMaster");
        run.remove("Vision.Control");
    }
}

}
