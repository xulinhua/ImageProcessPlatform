/**
 * @file AuditLog.h
 * @brief 中控台审计日志。JSON Lines，按天一个文件，存 %APPDATA%\RemoteMaster\audit
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_AUDIT_LOG_H
#define VISION_CONTROL_AUDIT_LOG_H

#include <QDate>
#include <QDateTime>
#include <QString>
#include <vector>

namespace Vision {

/**
 * @class AuditLog
 * @brief 审计日志：追加事件、按日期区间查询、按保留天数清理旧文件
 * @note 操作员取当前 Windows 用户名，构造时确定
 */
class AuditLog {
public:
    /** @brief 一条审计记录，对应文件里的一行 JSON */
    struct Entry {
        QDateTime time;     ///< 记录时间（本地时间，ISO 格式存盘）
        QString event;      ///< 事件类型，如 session.start，见 add() 说明
        QString device;     ///< 设备显示名
        QString deviceId;   ///< 设备 ID 或绑定键，可空
        QString op;         ///< 操作员（控制端 Windows 用户名）
        QString source;     ///< 共享源名，可空
        QString detail;     ///< 附加说明，可空
        qint64 seconds;     ///< 持续秒数，只有结束类事件大于 0
        /** @brief 秒数清零 */
        Entry() : seconds(0) {}
    };

    /** @brief 定位并创建审计目录，记下当前用户名 */
    AuditLog();

    /**
     * @brief 追加一条
     * @param event session.start、session.end、operate.start、operate.end、operate.reject、
     *              config.push、screenshot、device.add、device.remove
     */
    void add(const QString& event, const QString& device, const QString& deviceId, const QString& source,
        const QString& detail, qint64 seconds = 0);

    /**
     * @brief 查询 [from, to] 两端都含的记录
     * @return 按时间倒序（新的在前）
     */
    std::vector<Entry> query(const QDate& from, const QDate& to) const;

    /** @brief 删掉 days 天以前的文件；days <= 0 不清理 */
    void prune(int days) const;

    /** @brief 审计目录绝对路径 */
    QString dir() const;

    /** @brief 事件类型的中文名，未知类型原样返回 */
    static QString title(const QString& event);
    /** @brief 事件在界面上的色调名（green、blue、red、orange、purple、gray） */
    static const char* tone(const QString& event);
    /** @brief 秒数格式化成「x 秒」「x 分 y 秒」「x 小时 y 分」 */
    static QString duration(qint64 seconds);

private:
    /** @brief 某天对应的日志文件路径 yyyy-MM-dd.jsonl */
    QString fileOf(const QDate& day) const;

    QString m_dir;  ///< 审计目录 %APPDATA%\RemoteMaster\audit
    QString m_op;   ///< 操作员名，写入每条记录的 op
};

}

#endif
