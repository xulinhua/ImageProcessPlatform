/**
 * @file open_panel.h
 * @brief 按传输切换打开参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HS_IPC_DEBUG_OPEN_PANEL_H
#define HS_IPC_DEBUG_OPEN_PANEL_H

#include <QWidget>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;

/** @brief 左上角：传输、通道名，以及该传输才需要的参数 */
class OpenPanel : public QWidget {
public:
    explicit OpenPanel(QWidget* parent = 0);

    int transport() const;
    QString channelName() const;
    QString host() const;
    int port() const;
    int capacity() const;
    /** @brief 图像槽字节。界面按 MB 填写 */
    uint32_t imageSlotBytes() const;

    QPushButton* openButton() const { return m_open; }
    QPushButton* closeButton() const { return m_close; }

    /** @brief 打开后锁住参数，关闭后放开 */
    void setOpened(bool opened, const QString& error);

private:
    void onTransport(int index);

    QComboBox* m_transport;
    QLineEdit* m_name;
    QWidget* m_shm;
    QWidget* m_socket;
    QSpinBox* m_capacity;
    QSpinBox* m_slotMb;
    QLineEdit* m_host;
    QSpinBox* m_port;
    QPushButton* m_open;
    QPushButton* m_close;
    QLabel* m_status;
};

#endif
