/**
 * @file DeviceStore.h
 * @brief 中控台的设备列表：分组、备注、标签、最近连接。存 %APPDATA%\RemoteMaster\devices.json
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_DEVICE_STORE_H
#define VISION_CONTROL_DEVICE_STORE_H

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <vector>

namespace Vision {

/**
 * @class DeviceStore
 * @brief 设备库：设备条目与分组的增删改、排序和 JSON 持久化
 * @note 修改后不自动存盘，由调用方 save()
 */
class DeviceStore {
public:
    /** @brief key 为被控端的设备 ID；被控端没报 ID 时用地址和共享源拼出的绑定键 */
    struct Device {
        QString key;            ///< 设备 ID 或绑定键，非空，库内唯一
        QString alias;          ///< 用户起的别名，可空，显示时优先
        QString name;           ///< 被控端上报的机器名
        QString group;          ///< 所属分组名，空为未分组
        QString remark;         ///< 备注
        QStringList tags;       ///< 标签
        QString address;        ///< 最近一次上线的地址
        QString mac;            ///< 网卡 MAC，远程唤醒用
        QString system;         ///< 被控端上报的系统描述
        QDateTime lastSeen;     ///< 最近上线时间，touch() 时刷新
        QDateTime lastOpened;   ///< 最近打开会话窗口的时间，可无效
    };

    /** @brief 定位 devices.json，不读盘 */
    DeviceStore();

    /************************************************** 存取 **************************************************/
    /** @brief 清空后从 devices.json 读取；设备里出现的分组会补进分组表 */
    void load();
    /** @brief 原子写回 devices.json */
    void save() const;

    /************************************************** 分组 **************************************************/
    /** @brief 分组名列表，按添加顺序 */
    const QStringList& groups() const;
    /** @brief 新建分组；名字为空或重复返回 false */
    bool addGroup(const QString& name);
    /** @brief 改分组名并同步组内设备；目标名为空或已存在时不改 */
    void renameGroup(const QString& from, const QString& to);
    /** @brief 删分组，组内设备变为未分组 */
    void removeGroup(const QString& name);

    /************************************************** 设备 **************************************************/
    /** @brief 全部设备，顺序即界面顺序 */
    std::vector<Device>& devices();
    /** @brief 全部设备（只读） */
    const std::vector<Device>& devices() const;
    /** @brief 按 key 查找，没有返回空；指针在增删设备后失效 */
    Device* find(const QString& key);
    /** @brief 按 key 查找（只读），没有返回空 */
    const Device* find(const QString& key) const;

    /** @brief 设备上线时调用，没有就新建。返回是否新建 */
    bool touch(const QString& key, const QString& name, const QString& address, const QString& system);
    /**
     * @brief 把按 IP 登记的 d:地址 条目归到被控端上报的设备 ID 下，避免手动登记的那条一直离线、另起一条重复项
     * @param deviceId 被控端上报的设备 ID，空时不处理
     * @param address 该连接的对端 IP，与登记时填的地址按原文比较
     * @return 有条目改了 key 或被合并删除时返回 true，调用方负责 save()
     * @note 没有 ID 条目时直接改 key；两条都在时别名、分组、备注以 ID 条目为准、空的从 d: 条目补，标签取并集，再删 d: 条目。
     *       d: 条目从未上线过时，其 name 是用户手填的名称，没有别名就转成别名，免得被上报的机器名覆盖
     */
    bool adoptAddress(const QString& deviceId, const QString& address);
    /** @brief 删除设备，不存在时什么都不做 */
    void remove(const QString& key);

    /** @brief 把 key 挪到 before 前面；before 为空时挪到最后 */
    void moveBefore(const QString& key, const QString& before);

    /** @brief 显示名：别名 > 机器名 > 地址 > key */
    QString displayName(const Device& device) const;

    /** @brief 有设备 ID 用 ID，否则与 ControlSession 的绑定键同样拼法 */
    static QString keyFor(const QString& deviceId, int share, const QString& address, const QString& source);

private:
    QString m_path;                 ///< devices.json 绝对路径
    QStringList m_groups;           ///< 分组名，有序且不重复
    std::vector<Device> m_devices;  ///< 设备列表，有序
};

}

#endif
