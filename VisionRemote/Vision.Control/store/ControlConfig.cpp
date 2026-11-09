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
#include "RemoteLog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
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
    , shotDir(RemoteFiles::imageDir())
    , recordDir(RemoteFiles::videoDir())
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
    , streamCodec(1)
    , adaptive(true)
    , idleMinutes(30)
    , bandwidthKbps(0)
    , watermarkOn(false)
    , customLongEdge(1280)
    , customFps(30)
    , customBitrateKbps(2000)
    , udpPunch(false)
    , highestPrivilege(true)
    , logDisk(false)
    , logPictureDisk(false)
    , logRotateMinutes(60)
    , logKeepDays(7)
    , m_path(RemoteFiles::configFile("control.json"))
{
    connectOrder << "reverse" << "direct" << "relay";
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
    idleMinutes = obj.value("idleMinutes").toInt(idleMinutes);
    bandwidthKbps = obj.value("bandwidthKbps").toInt(bandwidthKbps);
    watermarkOn = obj.value("watermarkOn").toBool(watermarkOn);
    watermarkText = obj.value("watermarkText").toString(watermarkText);
    customLongEdge = obj.value("customLongEdge").toInt(customLongEdge);
    customFps = obj.value("customFps").toInt(customFps);
    customBitrateKbps = obj.value("customBitrateKbps").toInt(customBitrateKbps);
    udpPunch = obj.value("udpPunch").toBool(udpPunch);
    highestPrivilege = obj.value("highestPrivilege").toBool(true);
    logDisk = obj.value("logDisk").toBool(false);
    logPictureDisk = obj.value("logPictureDisk").toBool(false);
    logRotateMinutes = obj.value("logRotateMinutes").toInt(60);
    logKeepDays = obj.value("logKeepDays").toInt(7);
    if (logRotateMinutes < 15) {
        logRotateMinutes = 15;
    }
    if (logKeepDays < 1) {
        logKeepDays = 1;
    }
    if (shotDir.trimmed().isEmpty()) {
        shotDir = RemoteFiles::imageDir();
    }
    if (recordDir.trimmed().isEmpty()) {
        recordDir = RemoteFiles::videoDir();
    }
    const QJsonArray order = obj.value("connectOrder").toArray();
    if (!order.isEmpty()) {
        connectOrder.clear();
        for (int i = 0; i < order.size(); ++i) {
            const QString step = order.at(i).toString();
            if (step == "reverse" || step == "direct" || step == "udp" || step == "relay") {
                connectOrder.append(step);
            }
        }
    }
    if (connectOrder.isEmpty()) {
        connectOrder << "reverse" << "direct" << "relay";
    }
    if (udpPunch && !connectOrder.contains("udp")) {
        const int at = connectOrder.indexOf("direct");
        connectOrder.insert(at >= 0 ? at : connectOrder.size(), "udp");
    }
    if (defaultFps < RemoteConst::kMaxFpsMin) {
        defaultFps = RemoteConst::kMaxFpsMin;
    }
    if (defaultFps > RemoteConst::kMaxFpsMax) {
        defaultFps = RemoteConst::kMaxFpsMax;
    }
    if (defaultQuality < 0 || defaultQuality > 3) {
        defaultQuality = 1;
    }
    if (streamCodec < 0 || streamCodec > 3) {
        streamCodec = 0;
    }
    if (customLongEdge < 320) {
        customLongEdge = 320;
    }
    if (customFps < RemoteConst::kMaxFpsMin) {
        customFps = RemoteConst::kMaxFpsMin;
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
    obj.insert("idleMinutes", idleMinutes);
    obj.insert("bandwidthKbps", bandwidthKbps);
    obj.insert("watermarkOn", watermarkOn);
    obj.insert("watermarkText", watermarkText);
    obj.insert("customLongEdge", customLongEdge);
    obj.insert("customFps", customFps);
    obj.insert("customBitrateKbps", customBitrateKbps);
    obj.insert("udpPunch", udpPunch);
    obj.insert("highestPrivilege", highestPrivilege);
    obj.insert("logDisk", logDisk);
    obj.insert("logPictureDisk", logPictureDisk);
    obj.insert("logRotateMinutes", logRotateMinutes);
    obj.insert("logKeepDays", logKeepDays);
    QJsonArray order;
    for (int i = 0; i < connectOrder.size(); ++i) {
        order.append(connectOrder.at(i));
    }
    obj.insert("connectOrder", order);
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

bool ControlConfig::effectiveHighestPrivilege(int deviceTriState, bool accountOn) const
{
    if (highestPrivilege) {
        return true;
    }
    if (deviceTriState == 1) {
        return true;
    }
    if (deviceTriState == 0) {
        return false;
    }
    return accountOn;
}

void ControlConfig::applyRunLog() const
{
    RemoteLog::applyDisk(logDisk, logPictureDisk, logRotateMinutes, logKeepDays);
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
