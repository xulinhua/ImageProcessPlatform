/**
 * @file FileTransferDialog.h
 * @brief 双栏文件传输：左边本机，右边被控端
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_FILE_TRANSFER_DIALOG_H
#define VISION_CONTROL_FILE_TRANSFER_DIALOG_H

#include "ControlSession.h"
#include <QDialog>
#include <QList>
#include <string>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;

namespace Vision {

class AuditLog;

/**
 * @class FileTransferDialog
 * @brief 左右各选一个目录，把选中的文件传到另一边
 * @note 一次只跑一个文件，其余排在队列里。选中文件夹会连同里面的子目录一起传
 */
class FileTransferDialog : public QDialog {
public:
    /**
     * @brief 打开时两边都停在根：本机是磁盘列表，被控端是允许的根目录
     * @param peerId 当前会话的连接号
     */
    FileTransferDialog(ControlSession* session, AuditLog* audit, const std::string& peerId,
        const QString& deviceName, const QString& deviceId, QWidget* parent = 0);

private:
    /** @brief 队列里的一步：传一个文件、建一个目录，或列出远端目录再往下排 */
    enum JobKind {
        KindUpload = 1,
        KindDownload = 2,
        KindMkdir = 3,
        KindList = 4
    };
    struct Job {
        int kind;               ///< JobKind
        QString local;          ///< 本机完整路径
        QString remote;         ///< 被控端完整路径
        uint64_t resume;        ///< 下载续传位置，上传为 0
        uint64_t bytes;         ///< 文件大小，未知为 0
    };

    /************************************************** 界面 **************************************************/
    /** @brief 搭左右两栏、中间两个方向按钮和底部进度 */
    void build();
    /** @brief 往表格加一行。kind 为 D 或 F */
    void addRow(QTableWidget* table, const QString& kind, const QString& name, quint64 size, const QString& time);
    /** @brief 按当前选中项启用「传到被控端 / 传到本机」 */
    void syncButtons();

    /************************************************** 浏览 **************************************************/
    /** @brief 重读本机当前目录 */
    void reloadLocal();
    /** @brief 向被控端要当前目录 */
    void requestRemote();
    /** @brief 用被控端回的列表填右栏。E 行只进状态文字 */
    void fillRemote(const QString& listing);
    /** @brief 双击进入文件夹。localSide 为 true 时走本机 */
    void enterRow(bool localSide, int row);
    /** @brief 回到上一级。本机顶层是磁盘列表，被控端顶层是根目录 */
    void goUp(bool localSide);
    /** @brief 地址栏回车后进入输入的路径 */
    void commitPath(bool localSide);
    /** @brief 目录加子项，补上反斜杠 */
    QString childPath(const QString& dir, const QString& name) const;

    /************************************************** 传输 **************************************************/
    /** @brief 把左栏选中的文件或文件夹排进上传队列 */
    void uploadSelected();
    /** @brief 把右栏选中的文件或文件夹排进下载队列 */
    void downloadSelected();
    /** @brief 本机目录整夹上传，先建远端目录再排里面的内容 */
    void enqueueUploadDir(const QString& local, const QString& remote);
    /** @brief 排一个上传文件。目标已存在时按冲突策略处理 */
    void enqueueUploadFile(const QString& local, const QString& remote, quint64 bytes);
    /** @brief 远端目录整夹下载。先在本机建目录，再列远端 */
    void enqueueDownloadDir(const QString& remote, const QString& local);
    /** @brief 排一个下载文件 */
    void enqueueDownloadFile(const QString& remote, const QString& local, quint64 bytes);
    /** @brief 列目录的结果到了，把子目录和文件排进队列 */
    void takeWalk(const QString& listing);
    /** @brief 队列空了就刷新两边，让新建的文件夹出现 */
    void finishBatchView();
    /** @brief 地址栏或顶层磁盘上的一项，收成文件夹名 */
    QString folderName(const QString& path) const;
    /** @brief 当前栏里这一项的完整路径。顶层磁盘、根目录本身就是完整路径 */
    QString itemPath(bool localSide, const QString& name) const;
    /**
     * @brief 目标已存在时问覆盖、重命名、续传或跳过
     * @param remoteSide true 表示目标在被控端，不提供续传
     * @return 跳过或无法处理时返回空
     */
    QString choosePath(const QString& path, bool remoteSide, quint64 have, quint64 total, uint64_t* resume);
    /** @brief 当前目录或队列里是否已经有这个文件名 */
    bool nameTaken(bool remoteSide, const QString& fileName) const;
    /** @brief path 的文件名加上 (n)，目录部分不变 */
    QString indexed(const QString& path, int n) const;
    /** @brief 没有正在传的文件时，取出队列头开始下一次 */
    void pump();
    /** @brief 定时刷新目录和进度。传完一个就接着队列里的下一个 */
    void tick();
    /** @brief 找到当前这条连接。没有返回 false */
    bool copyTile(ControlSession::Tile* out) const;
    /** @brief 字节数显示成 B / KB / MB / GB */
    QString sizeText(quint64 n) const;

    ControlSession* m_session;      ///< 收发会话，不拥有
    AuditLog* m_audit;              ///< 审计，不拥有
    std::string m_peerId;           ///< 这条会话的连接号
    QString m_name;                 ///< 设备显示名，写审计
    QString m_deviceId;             ///< 设备 ID，写审计
    QString m_localDir;             ///< 本机当前目录，空表示磁盘列表
    QString m_remoteDir;            ///< 被控端当前目录，空表示根目录
    uint64_t m_dirSeen;             ///< 已经填过的远端目录序号
    uint64_t m_waitSeq;             ///< 开始这次传输时的 fileSeq
    uint32_t m_nextId;              ///< 下一个传输号
    bool m_waiting;                 ///< 有一步正在进行
    bool m_waitUpload;              ///< 正在传的是上传，结束后刷新被控端
    bool m_waitList;                ///< 正在等一次列目录，这包结果不刷新右栏
    int m_conflict;                 ///< 0 每次询问，1 全部覆盖，2 全部跳过
    uint64_t m_walkSeen;            ///< 开始列目录时的序号
    QString m_walkLocal;            ///< 这次列目录要落到的本机目录
    QString m_walkRemote;           ///< 这次正在列的被控端目录
    QList<QString> m_walked;        ///< 这次下载已经展开过的远端目录，避免链接绕回去
    QList<Job> m_queue;             ///< 还没开始的步骤

    QLineEdit* m_localPath;         ///< 本机地址栏
    QLineEdit* m_remotePath;        ///< 被控端地址栏
    QTableWidget* m_local;          ///< 本机文件表
    QTableWidget* m_remote;         ///< 被控端文件表
    QPushButton* m_toRemote;        ///< 传到被控端
    QPushButton* m_toLocal;         ///< 传到本机
    QLabel* m_status;               ///< 底部说明
    QProgressBar* m_progress;       ///< 当前文件进度
    QTimer* m_timer;                ///< 200 ms 刷新
};

}

#endif
