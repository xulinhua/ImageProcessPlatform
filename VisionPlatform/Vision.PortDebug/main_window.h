/**
 * @file main_window.h
 * @brief 通讯调试器主窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 一种通讯方式一套参数。下拉切换，共用菜单、状态栏、报文和日志。
 */

#ifndef HS_COMM_DEBUG_MAIN_WINDOW_H
#define HS_COMM_DEBUG_MAIN_WINDOW_H

#include <QMainWindow>
#include <QVector>

class ChannelPage;
class ChannelSession;
class QPlainTextEdit;
class QTableWidget;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
class QStackedWidget;
class QPushButton;
class QWidget;
class QFile;
class QFrame;
class QGridLayout;

/** @brief 上为通讯方式下拉和参数，中为报文和日志，下为发送，右为客户端列表 */
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private:
    void onOpen();
    void onClose();
    void onSend();
    void onHttp();
    void drain();
    void showPage(int index);
    void onKind(int index);
    void clearSessionView();
    ChannelPage* currentPage() const;
    bool buildPayload(const QString& text, bool hex, QByteArray* out, QString* err) const;
    void dispatchBytes(const QByteArray& bytes, const QString& shown);
    void appendMessage(const QString& text);
    void writeLog(const QString& text);
    void noteHistory(const QString& text);
    void sendPreset(int index);
    void cyclePresets();
    void sendFileChunk();
    void readOnce();
    void pollModbus();
    void writeTable(bool oneRow, int row);
    void fillModbusTable(int fn, int address, const QString& values);
    void refreshCounts();
    void refreshPeers();
    void loadDesk();
    void saveDesk();
    void buildMenu();
    void saveText(QPlainTextEdit* edit, const QString& title);
    static QFrame* makePanel(const QString& title, QWidget* body, QWidget* parent);
    static void polish(QLabel* lab);
    void useExeIcon();
    void setPresetCount(int count);

    QComboBox* m_kind;
    bool m_kindGuard;
    QStackedWidget* m_pages;
    QPushButton* m_open;
    QPushButton* m_close;
    QLabel* m_chip;
    QPlainTextEdit* m_recv;
    QLabel* m_recvTitle;
    QPlainTextEdit* m_log;
    QWidget* m_byteSend;
    QWidget* m_httpSend;
    QWidget* m_modbusBar;
    QTableWidget* m_regs;
    QWidget* m_peerBox;
    QTableWidget* m_peers;
    QComboBox* m_target;
    QPlainTextEdit* m_send;
    QCheckBox* m_hexView;
    QCheckBox* m_stamp;
    QCheckBox* m_pause;
    QCheckBox* m_scroll;
    QCheckBox* m_hexSend;
    QCheckBox* m_crlf;
    QCheckBox* m_at;
    QCheckBox* m_escape;
    QComboBox* m_check;
    QSpinBox* m_checkFrom;
    QSpinBox* m_checkTail;
    QComboBox* m_codec;
    QComboBox* m_history;
    QCheckBox* m_auto;
    QSpinBox* m_interval;
    QPushButton* m_fileSend;
    QComboBox* m_fileGap;
    QGridLayout* m_presetGrid;
    QWidget* m_presetBody;
    QSpinBox* m_presetCount;
    QVector<QLineEdit*> m_presetName;
    QVector<QCheckBox*> m_presetHex;
    QVector<QLineEdit*> m_presetEdit;
    QVector<QSpinBox*> m_presetMs;
    QVector<QPushButton*> m_presetSend;
    QCheckBox* m_cycle;
    QTimer* m_cycleTimer;
    int m_cycleIndex;
    QComboBox* m_method;
    QTableWidget* m_headers;
    QPlainTextEdit* m_body;
    QComboBox* m_httpHistory;
    QSpinBox* m_slave;
    QComboBox* m_fn;
    QSpinBox* m_address;
    QSpinBox* m_count;
    QSpinBox* m_period;
    QComboBox* m_display;
    QPushButton* m_poll;
    QLabel* m_where;
    QLabel* m_counts;
    QPushButton* m_wsDrop;
    ChannelSession* m_session;
    QTimer* m_timer;
    QTimer* m_autoTimer;
    QTimer* m_pollTimer;
    QFile* m_dump;
    bool m_waitOpen;
    bool m_wasOpen;
    bool m_fileBusy;
    QByteArray m_fileBytes;
    int m_filePos;
    qint64 m_openedAt;
    QString m_lastUdp;
    bool m_lastDtr;
    bool m_lastRts;
    int m_httpCode;
    int m_httpMs;
};

#endif
