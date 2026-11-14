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
#include "RemotePorts.h"
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
    , port(RemotePorts::listen())
    , trayOnClose(true)
    , startHidden(false)
    , autoStart(false)
    , cleanOnStart(true)
    , cleanNotify(true)
    , discovery(true)
    , fitDefault(true)
    , shotDir(RemoteFiles::imageDir())
    , recordDir(RemoteFiles::videoDir())
    , autoRecord(false)
    , recordDays(90)
    , auditDays(180)
    , tourSeconds(10)
    , relayOn(false)
    , startRelay(true)
    , relayPort(RemotePorts::relay())
    , accessHost(QString::fromUtf8(RemotePorts::accessHost()))
    , accessPort(443)
    , rows(3)
    , cols(3)
    , defaultFps(RemoteConst::kMaxFpsDefault)
    , defaultQuality(1)
    , streamCodec(1)
    , adaptive(true)
    , resolutionMode(0)
    , resolutionWidth(1920)
    , resolutionHeight(1080)
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
    cleanOnStart = obj.value("cleanOnStart").toBool(true);
    cleanNotify = obj.value("cleanNotify").toBool(true);
    discovery = obj.value("discovery").toBool(discovery);
    fitDefault = obj.value("fitDefault").toBool(fitDefault);
    shotDir = obj.value("shotDir").toString(shotDir);
    recordDir = obj.value("recordDir").toString(recordDir);
    autoRecord = obj.value("autoRecord").toBool(autoRecord);
    recordDays = obj.value("recordDays").toInt(recordDays);
    auditDays = obj.value("auditDays").toInt(auditDays);
    tourSeconds = obj.value("tourSeconds").toInt(tourSeconds);
    relayOn = obj.value("relayOn").toBool(relayOn);
    startRelay = obj.value("startRelay").toBool(true);
    relayHost = obj.value("relayHost").toString(relayHost);
    relayPort = obj.value("relayPort").toInt(relayPort);
    relayKey = obj.value("relayKey").toString(relayKey);
    accessHost = obj.value("accessHost").toString(accessHost);
    accessPort = obj.value("accessPort").toInt(accessPort);
    rows = obj.value("rows").toInt(rows);
    cols = obj.value("cols").toInt(cols);
    defaultFps = obj.value("defaultFps").toInt(defaultFps);
    defaultQuality = obj.value("defaultQuality").toInt(defaultQuality);
    streamCodec = obj.value("streamCodec").toInt(streamCodec);
    adaptive = obj.value("adaptive").toBool(adaptive);
    resolutionMode = obj.value("resolutionMode").toInt(resolutionMode);
    resolutionWidth = obj.value("resolutionWidth").toInt(resolutionWidth);
    resolutionHeight = obj.value("resolutionHeight").toInt(resolutionHeight);
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
    if (accessHost.trimmed().isEmpty()) {
        accessHost = QString::fromUtf8(RemotePorts::accessHost());
    }
    if (accessPort < 1 || accessPort > 65535) {
        accessPort = 443;
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
    if (streamCodec < 0 || streamCodec > 4) {
        streamCodec = 0;
    }
    if (resolutionMode < 0 || resolutionMode > 2) {
        resolutionMode = 0;
    }
    if (resolutionWidth < 320) {
        resolutionWidth = 320;
    }
    if (resolutionWidth > 7680) {
        resolutionWidth = 7680;
    }
    if (resolutionHeight < 240) {
        resolutionHeight = 240;
    }
    if (resolutionHeight > 4320) {
        resolutionHeight = 4320;
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
    obj.insert("cleanOnStart", cleanOnStart);
    obj.insert("cleanNotify", cleanNotify);
    obj.insert("discovery", discovery);
    obj.insert("fitDefault", fitDefault);
    obj.insert("shotDir", shotDir);
    obj.insert("recordDir", recordDir);
    obj.insert("autoRecord", autoRecord);
    obj.insert("recordDays", recordDays);
    obj.insert("auditDays", auditDays);
    obj.insert("tourSeconds", tourSeconds);
    obj.insert("relayOn", relayOn);
    obj.insert("startRelay", startRelay);
    obj.insert("relayHost", relayHost);
    obj.insert("relayPort", relayPort);
    obj.insert("relayKey", relayKey);
    obj.insert("accessHost", accessHost);
    obj.insert("accessPort", accessPort);
    obj.insert("rows", rows);
    obj.insert("cols", cols);
    obj.insert("defaultFps", defaultFps);
    obj.insert("defaultQuality", defaultQuality);
    obj.insert("streamCodec", streamCodec);
    obj.insert("adaptive", adaptive);
    obj.insert("resolutionMode", resolutionMode);
    obj.insert("resolutionWidth", resolutionWidth);
    obj.insert("resolutionHeight", resolutionHeight);
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

int ControlConfig::pictureLongEdge(int qualityIndex) const
{
    if (resolutionMode == 1) {
        return RemoteConst::kNativeEdge;
    }
    if (resolutionMode == 2) {
        int edge = resolutionWidth > resolutionHeight ? resolutionWidth : resolutionHeight;
        if (edge < 320) {
            edge = 320;
        }
        if (edge > 7680) {
            edge = 7680;
        }
        return edge;
    }
    if (qualityIndex <= 0) {
        return 960;
    }
    if (qualityIndex == 3) {
        int edge = customLongEdge < 320 ? 320 : customLongEdge;
        if (edge > 7680) {
            edge = 7680;
        }
        return edge;
    }
    if (qualityIndex >= 2) {
        return 1600;
    }
    return RemoteConst::kLongEdge;
}

bool ControlConfig::lockPictureEdge() const
{
    return resolutionMode == 1 || resolutionMode == 2;
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
