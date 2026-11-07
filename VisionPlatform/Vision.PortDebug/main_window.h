/**
 * @file main_window.h
 * @brief 通讯调试器主窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HS_COMM_DEBUG_MAIN_WINDOW_H
#define HS_COMM_DEBUG_MAIN_WINDOW_H

#include <QMainWindow>

class ChannelPage;
class QShortcut;
class ChannelSession;
class QPlainTextEdit;
class QListWidget;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
class QStackedWidget;
class QTabBar;
class QPushButton;
class QToolButton;

/** @brief 上为通道参数，中为接收，下为发送，右为对端 */
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private:
    void onOpen();
    void onClose();
    void onSend();
    void drain();
    void showPage(int index);
    ChannelPage* currentPage() const;
    QString selectedPeer() const;
    /** @brief 按当前的 HEX、转义、回车换行、编码和校验，把一条文本收成要发出的字节 */
    bool buildPayload(const QString& text, bool hex, QByteArray* out, QString* err) const;
    /** @brief 字节通道和 WebSocket 的发送。没打开或没选客户端时不发 */
    void dispatchBytes(const QByteArray& bytes, bool asText, const QString& shown);
    void noteHistory(const QString& text);
    void sendPreset(int index);
    void cyclePresets();
    void loadDesk();
    void saveDesk();
    void refreshCounts();
    static const int kPresets = 6;

    QTabBar* m_tabs;
    QStackedWidget* m_pages;
    QPushButton* m_open;
    QPushButton* m_close;
    QLabel* m_status;
    QPlainTextEdit* m_recv;
    QPlainTextEdit* m_send;
    QCheckBox* m_hexView;
    QCheckBox* m_stamp;
    QCheckBox* m_hexSend;
    QCheckBox* m_crlf;
    QCheckBox* m_escape;
    QComboBox* m_check;
    QComboBox* m_codec;
    QCheckBox* m_pause;
    QComboBox* m_history;
    QCheckBox* m_auto;
    QSpinBox* m_interval;
    QLineEdit* m_presetEdit[6];
    QCheckBox* m_presetHex[6];
    QCheckBox* m_cycle;
    QSpinBox* m_cycleMs;
    QTimer* m_cycleTimer;
    int m_cycleIndex;
    QLabel* m_counts;
    QWidget* m_modbusBar;
    QSpinBox* m_slave;
    QComboBox* m_fn;
    QSpinBox* m_address;
    QSpinBox* m_count;
    QWidget* m_httpBar;
    QComboBox* m_method;
    QPlainTextEdit* m_headers;
    QPushButton* m_disconnect;
    QToolButton* m_rawToggle;
    QPlainTextEdit* m_raw;
    QListWidget* m_peers;
    ChannelSession* m_session;
    QTimer* m_timer;
    QTimer* m_autoTimer;
    bool m_waitOpen;
};

#endif
