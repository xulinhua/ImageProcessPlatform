/**
 * @file main_window.h
 * @brief 进程通讯调试器主窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HS_IPC_DEBUG_MAIN_WINDOW_H
#define HS_IPC_DEBUG_MAIN_WINDOW_H

#include <QMainWindow>

class OpenPanel;
class ComposePanel;
class SessionWorker;
class QTableWidget;
class QLabel;
class QTimer;
class QCheckBox;
class QSpinBox;
class QPushButton;

/** @brief 左参数、下记录、右预览 */
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private:
    void onOpen();
    void onClose();
    void onSend();
    void drain();
    void showImage(const QByteArray& bytes, bool jpeg);

    OpenPanel* m_open;
    ComposePanel* m_compose;
    QTableWidget* m_log;
    QLabel* m_preview;
    SessionWorker* m_worker;
    QTimer* m_timer;
    QTimer* m_heartbeat;
    QCheckBox* m_beatOn;
    QSpinBox* m_beatMs;
    QLabel* m_idle;
    QLabel* m_state;
    QLabel* m_where;
    bool m_sawOpen;

    void useExeIcon();
    void saveLog();
    void sendHeartbeat();
    void note(const QString& summary);
    void buildMenu();
};

#endif
