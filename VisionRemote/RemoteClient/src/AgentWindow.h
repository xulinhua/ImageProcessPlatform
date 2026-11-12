/**
 * @file AgentWindow.h
 * @brief 被控端窗口。左上设备卡、右上会话、下方共享范围和右侧小预览，对齐 Windows 被控端
 */

#ifndef REMOTE_CLIENT_AGENT_WINDOW_H
#define REMOTE_CLIENT_AGENT_WINDOW_H

#include <QByteArray>
#include <QIcon>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;
class QWidget;

namespace Vision {

class AgentWindow : public QWidget {
    Q_OBJECT
public:
    /** @brief 先显示占位，身份就绪后由 applyStatus 填上 */
    explicit AgentWindow(QWidget* parent = 0);

public slots:
    /**
     * @brief 刷新设备号、指纹、桌面和状态
     * @note 从会话线程用队列调用
     */
    void applyStatus(const QString& deviceId, const QString& fingerprint, const QString& desk, const QString& state);
    /**
     * @brief 显示刚送出的一帧。jpeg 为空时预览区写「还没有送出画面」
     */
    void applyPreview(const QByteArray& jpeg, int w, int h);
    /** @brief 刷新进程下拉框。每一项是「类名\\t标题」 */
    void applyWindows(const QStringList& rows);
    /** @brief 中控台改了共享范围，单选和输入框跟着变 */
    void applyShare(bool desktop, const QString& process, const QString& title);
    /**
     * @brief 聊天记录追加一行。kind 为 1 时再弹出「中控台提示」
     */
    void applyChat(const QString& name, const QString& text, int kind);
    /** @brief 文件传输状态 */
    void applyFile(const QString& note);
    /** @brief 显示一次性验证码。每三位空一格，和 Windows 一样 */
    void applyCode(const QString& code);

signals:
    /** @brief 本机改了共享范围 */
    void shareChosen(bool desktop, const QString& process, const QString& title);
    /** @brief 本机发出一条聊天 */
    void chatSend(const QString& text);
    /** @brief 手动换一次验证码 */
    void codeRefresh();

protected:
    /** @brief 窗口变大时，按预览框重新缩那一帧 */
    void resizeEvent(QResizeEvent* event);

private:
    /** @brief 套上和 Windows 被控端同一套字体、调色板和样式表 */
    void applyTheme();
    /** @brief 改 tone 后重新套样式。gray、blue 等见样式表 */
    void setTone(QWidget* widget, const char* tone);
    /** @brief 蓝色卡片上的白色复制图标 */
    QIcon copyIcon() const;
    /** @brief 验证码旁边的刷新图标 */
    QIcon refreshIcon() const;
    /** @brief 验证码说明前的时钟图标 */
    QPixmap clockIcon() const;
    /** @brief 窗口和任务栏用 Windows 被控端同一套绿色图标 */
    void loadIcon();
    /** @brief 把当前设备号放进剪贴板 */
    void copyId();
    /** @brief 把证书指纹放进剪贴板 */
    void copyFingerprint();
    /** @brief 把最近一帧按预览框缩好贴上去 */
    void fitPreview();
    /** @brief 整屏时只能选显示器，进程画面时才能填进程和标题 */
    void syncShare();
    /** @brief 把当前单选和输入框交给会话 */
    void commitShare();
    /** @brief 进程下拉框停在当前进程名上，没有就补一项 */
    void selectProcess();
    /** @brief 把输入框里的字发出去 */
    void sendChat();
    /** @brief 类型为弹窗的聊天，置顶显示，点「知道了」关闭 */
    void showNotice(const QString& name, const QString& text);

    QLabel* m_id;
    QLabel* m_code;
    QLabel* m_fingerprint;
    QLabel* m_desk;
    QLabel* m_state;
    QLabel* m_dot;
    QLabel* m_count;
    QWidget* m_sessionRow;
    QLabel* m_sessionMeta;
    QLabel* m_sessionTag;
    QLabel* m_noSession;
    QLabel* m_previewInfo;
    QLabel* m_preview;
    QRadioButton* m_desktop;
    QRadioButton* m_window;
    QComboBox* m_monitor;
    QComboBox* m_processBox;
    QLineEdit* m_titleEdit;
    QPlainTextEdit* m_chat;
    QLineEdit* m_chatInput;
    QLabel* m_file;
    QPixmap m_frame;
    QString m_idText;
    QString m_fpText;
    bool m_desktopShare;        ///< 当前要整屏
    QString m_process;          ///< 当前进程名，对应 WM_CLASS
    QString m_title;            ///< 当前窗口标题
};

}

#endif
