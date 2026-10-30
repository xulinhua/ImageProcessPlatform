/**
 * @file DeviceStore.cpp
 * @brief 设备列表读写
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeviceStore.h"
#include "RemoteFiles.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace Vision {

DeviceStore::DeviceStore()
    : m_path(QDir(RemoteFiles::appDataDir("RemoteMaster")).filePath("devices.json"))
{
}

void DeviceStore::load()
{
    m_groups.clear();
    m_devices.clear();
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray groups = root.value("groups").toArray();
    for (int i = 0; i < groups.size(); ++i) {
        const QString name = groups.at(i).toString();
        if (!name.isEmpty() && !m_groups.contains(name)) {
            m_groups.append(name);
        }
    }
    const QJsonArray devices = root.value("devices").toArray();
    for (int i = 0; i < devices.size(); ++i) {
        const QJsonObject obj = devices.at(i).toObject();
        Device device;
        device.key = obj.value("key").toString();
        if (device.key.isEmpty()) {
            continue;
        }
        device.alias = obj.value("alias").toString();
        device.name = obj.value("name").toString();
        device.group = obj.value("group").toString();
        device.remark = obj.value("remark").toString();
        const QJsonArray tags = obj.value("tags").toArray();
        for (int t = 0; t < tags.size(); ++t) {
            device.tags.append(tags.at(t).toString());
        }
        device.address = obj.value("address").toString();
        device.mac = obj.value("mac").toString();
        device.system = obj.value("system").toString();
        device.lastSeen = QDateTime::fromString(obj.value("lastSeen").toString(), Qt::ISODate);
        device.lastOpened = QDateTime::fromString(obj.value("lastOpened").toString(), Qt::ISODate);
        if (!device.group.isEmpty() && !m_groups.contains(device.group)) {
            m_groups.append(device.group);
        }
        m_devices.push_back(device);
    }
}

void DeviceStore::save() const
{
    QJsonArray groups;
    for (int i = 0; i < m_groups.size(); ++i) {
        groups.append(m_groups.at(i));
    }
    QJsonArray devices;
    for (size_t i = 0; i < m_devices.size(); ++i) {
        const Device& device = m_devices[i];
        QJsonObject obj;
        obj.insert("key", device.key);
        obj.insert("alias", device.alias);
        obj.insert("name", device.name);
        obj.insert("group", device.group);
        obj.insert("remark", device.remark);
        obj.insert("tags", QJsonArray::fromStringList(device.tags));
        obj.insert("address", device.address);
        obj.insert("mac", device.mac);
        obj.insert("system", device.system);
        obj.insert("lastSeen", device.lastSeen.toString(Qt::ISODate));
        obj.insert("lastOpened", device.lastOpened.toString(Qt::ISODate));
        devices.append(obj);
    }
    QJsonObject root;
    root.insert("groups", groups);
    root.insert("devices", devices);
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.commit();
    }
}

const QStringList& DeviceStore::groups() const
{
    return m_groups;
}

bool DeviceStore::addGroup(const QString& name)
{
    const QString text = name.trimmed();
    if (text.isEmpty() || m_groups.contains(text)) {
        return false;
    }
    m_groups.append(text);
    return true;
}

void DeviceStore::renameGroup(const QString& from, const QString& to)
{
    const QString text = to.trimmed();
    const int index = m_groups.indexOf(from);
    if (index < 0 || text.isEmpty() || m_groups.contains(text)) {
        return;
    }
    m_groups[index] = text;
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].group == from) {
            m_devices[i].group = text;
        }
    }
}

void DeviceStore::removeGroup(const QString& name)
{
    m_groups.removeAll(name);
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].group == name) {
            m_devices[i].group.clear();
        }
    }
}

std::vector<DeviceStore::Device>& DeviceStore::devices()
{
    return m_devices;
}

const std::vector<DeviceStore::Device>& DeviceStore::devices() const
{
    return m_devices;
}

DeviceStore::Device* DeviceStore::find(const QString& key)
{
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].key == key) {
            return &m_devices[i];
        }
    }
    return 0;
}

const DeviceStore::Device* DeviceStore::find(const QString& key) const
{
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].key == key) {
            return &m_devices[i];
        }
    }
    return 0;
}

bool DeviceStore::touch(const QString& key, const QString& name, const QString& address, const QString& system)
{
    Device* device = find(key);
    bool created = false;
    if (!device) {
        Device fresh;
        fresh.key = key;
        m_devices.push_back(fresh);
        device = &m_devices.back();
        created = true;
    }
    if (!name.isEmpty()) {
        device->name = name;
    }
    if (!address.isEmpty()) {
        device->address = address;
    }
    if (!system.isEmpty()) {
        device->system = system;
    }
    device->lastSeen = QDateTime::currentDateTime();
    return created;
}

bool DeviceStore::adoptAddress(const QString& deviceId, const QString& address)
{
    if (deviceId.isEmpty() || address.isEmpty()) {
        return false;
    }
    const QString manual = keyFor(QString(), 1, address, QString());
    if (manual == deviceId) {
        return false;
    }
    Device* old = find(manual);
    if (!old) {
        return false;
    }
    if (!old->lastSeen.isValid() && old->alias.isEmpty()) {
        old->alias = old->name;
    }
    Device* target = find(deviceId);
    if (!target) {
        old->key = deviceId;
        return true;
    }
    if (target->alias.isEmpty()) {
        target->alias = old->alias;
    }
    if (target->group.isEmpty()) {
        target->group = old->group;
    }
    if (target->remark.isEmpty()) {
        target->remark = old->remark;
    }
    for (int i = 0; i < old->tags.size(); ++i) {
        if (!target->tags.contains(old->tags.at(i))) {
            target->tags.append(old->tags.at(i));
        }
    }
    if (old->lastOpened.isValid() && (!target->lastOpened.isValid() || old->lastOpened > target->lastOpened)) {
        target->lastOpened = old->lastOpened;
    }
    remove(manual);
    return true;
}

void DeviceStore::remove(const QString& key)
{
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].key == key) {
            m_devices.erase(m_devices.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
}

void DeviceStore::moveBefore(const QString& key, const QString& before)
{
    if (key == before) {
        return;
    }
    Device* device = find(key);
    if (!device) {
        return;
    }
    const Device copy = *device;
    remove(key);
    if (before.isEmpty()) {
        m_devices.push_back(copy);
        return;
    }
    for (size_t i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].key == before) {
            m_devices.insert(m_devices.begin() + static_cast<std::ptrdiff_t>(i), copy);
            return;
        }
    }
    m_devices.push_back(copy);
}

QString DeviceStore::displayName(const Device& device) const
{
    if (!device.alias.isEmpty()) {
        return device.alias;
    }
    if (!device.name.isEmpty()) {
        return device.name;
    }
    return device.address.isEmpty() ? device.key : device.address;
}

QString DeviceStore::keyFor(const QString& deviceId, int share, const QString& address, const QString& source)
{
    if (!deviceId.isEmpty()) {
        return deviceId;
    }
    return share == 1 ? QString("d:") + address : QString("w:") + address + "\n" + source;
}

}
