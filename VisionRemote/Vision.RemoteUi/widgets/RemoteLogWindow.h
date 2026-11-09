/**
 * @file RemoteLogWindow.h
 * @brief 独立运行日志窗口：定时拉取 RemoteLog，按钮打开、清空、暂停、过滤
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_LOG_WINDOW_H
#define VISION_REMOTE_LOG_WINDOW_H

#include "RemoteLog.h"
#include <QDialog>
#include <cstdint>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace Vision {

/**
 * @class RemoteLogWindow
 * @brief 非模态日志窗。关闭只隐藏，再点按钮会重新 raise
 */
class RemoteLogWindow : public QDialog {
public:
    /**
     * @brief 建窗
     * @param title 窗口标题，如「中控台 · 运行日志」
     */
    explicit RemoteLogWindow(const QString& title, QWidget* parent = 0);

    /** @brief 显示并置顶；首次或被关后都可再调 */
    void showAndRaise();

protected:
    /** @brief 关窗只隐藏，不销毁，便于按钮再次打开 */
    void closeEvent(QCloseEvent* event) override;

private:
    /** @brief 从 RemoteLog 拉增量，按过滤条件追加到文本框 */
    void drain();
    /** @brief 是否保留该行（画面过滤开启时只留画面/解码/发送相关 tag） */
    bool acceptLine(const RemoteLog::Line& line) const;
    /** @brief 打开日志文件所在目录 */
    void openFolder();

    QPlainTextEdit* m_text;     ///< 日志正文
    QCheckBox* m_pause;         ///< 暂停接收
    QCheckBox* m_pictureOnly;   ///< 只看画面链路
    QLabel* m_hint;             ///< 底部提示：条数与文件路径
    QTimer* m_timer;            ///< 100 ms 拉取
    uint64_t m_seq;             ///< 已显示的最大序号
    int m_shown;                ///< 窗口内已显示行数
};

}

#endif
