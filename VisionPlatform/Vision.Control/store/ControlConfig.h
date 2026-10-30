/**
 * @file ControlConfig.h
 * @brief control.json：监听、托盘、发现、画面、截图、审计、监控墙布局
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_CONFIG_H
#define VISION_CONTROL_CONFIG_H

#include <QString>

namespace Vision {

/**
 * @class ControlConfig
 * @brief 控制端配置，存在 exe 同目录的 control.json
 * @note 字段公开，由界面直接读写，改完调 save()
 */
class ControlConfig {
public:
    /** @brief 填默认值：监听 0.0.0.0:默认端口、3x3 监控墙、审计保留 180 天 */
    ControlConfig();

    /** @brief 从 control.json 读取；文件缺失或字段缺失时保留当前值 */
    void load();
    /** @brief 原子写回 control.json */
    void save() const;

    /** @brief 按 autoStart 写或删 HKCU Run 项 */
    void applyAutoStart() const;
    /** @brief 删掉录屏目录里超过 recordDays 的 mp4 和 avi */
    void purgeRecords() const;

    QString host;       ///< 监听地址，默认 0.0.0.0
    int port;           ///< 监听端口，默认 RemoteConst::kPort
    bool trayOnClose;   ///< 关闭主窗时缩到托盘而不是退出
    bool startHidden;   ///< 启动时不显示主窗，只留托盘
    bool autoStart;     ///< 开机自启（HKCU Run 项）
    bool discovery;     ///< 是否开启局域网发现
    bool fitDefault;    ///< 会话窗口默认按窗口缩放画面
    QString shotDir;    ///< 截图保存目录，默认 %APPDATA%\RemoteMaster\screenshots
    QString recordDir;  ///< 录屏目录，空则用 shotDir
    bool autoRecord;    ///< 取得操作权后自动录屏
    int recordDays;     ///< 录屏保留天数
    int auditDays;      ///< 审计日志保留天数，<= 0 不清理
    int tourSeconds;    ///< 监控墙轮巡间隔（秒）
    bool relayOn;       ///< 反连和直连都没有时走中继
    QString relayHost;  ///< 中继地址
    int relayPort;      ///< 中继端口，默认 19200
    QString relayKey;   ///< 中继口令，与被控端一致
    int rows;           ///< 监控墙行数
    int cols;           ///< 监控墙列数
    int defaultFps;     ///< 新会话的帧率上限，5–500
    int defaultQuality; ///< 新会话画质：0 流畅，1 均衡，2 清晰
    int streamCodec;    ///< 发送编码：0 沿用被控端，1 JPEG，2 H.264，3 原图 BGRA
    bool adaptive;      ///< 延迟很高时自动略降分辨率，不把帧率打到 1

private:
    QString m_path;     ///< control.json 绝对路径（exe 同目录）
};

}

#endif
