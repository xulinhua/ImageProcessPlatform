/**
 * @file AgentFileTransferDialog.h
 * @brief 被控端双栏文件传输：左边本机，右边中控台
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_AGENT_FILE_TRANSFER_DIALOG_H
#define VISION_CONTROLLED_AGENT_FILE_TRANSFER_DIALOG_H

#include "AgentSession.h"
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

/**
 * @class AgentFileTransferDialog
 * @brief 与中控台 FileTransferDialog 同交互：左右栏互传
 */
class AgentFileTransferDialog : public QDialog {
public:
    explicit AgentFileTransferDialog(AgentSession* session, QWidget* parent = 0);

private:
    enum JobKind {
        KindUpload = 1,
        KindDownload = 2,
        KindMkdir = 3,
        KindList = 4
    };
    struct Job {
        int kind;
        QString local;
        QString remote;
        uint64_t resume;
        uint64_t bytes;
    };

    void build();
    void addRow(QTableWidget* table, const QString& kind, const QString& name, quint64 size, const QString& time);
    void syncButtons();
    void reloadLocal();
    void requestRemote();
    void fillRemote(const QString& listing);
    void enterRow(bool localSide, int row);
    void goUp(bool localSide);
    void commitPath(bool localSide);
    QString childPath(const QString& dir, const QString& name) const;
    void uploadSelected();
    void downloadSelected();
    void enqueueUploadDir(const QString& local, const QString& remote);
    void enqueueUploadFile(const QString& local, const QString& remote, quint64 bytes);
    void enqueueDownloadDir(const QString& remote, const QString& local);
    void enqueueDownloadFile(const QString& remote, const QString& local, quint64 bytes);
    void takeWalk(const QString& listing);
    void finishBatchView();
    QString folderName(const QString& path) const;
    QString itemPath(bool localSide, const QString& name) const;
    QString choosePath(const QString& path, bool remoteSide, quint64 have, quint64 total, uint64_t* resume);
    bool nameTaken(bool remoteSide, const QString& fileName) const;
    QString indexed(const QString& path, int n) const;
    void pump();
    void tick();
    QString sizeText(quint64 n) const;

    AgentSession* m_session;
    QString m_localDir;
    QString m_remoteDir;
    uint64_t m_dirSeen;
    uint64_t m_waitSeq;
    uint32_t m_nextId;
    bool m_waiting;
    bool m_waitUpload;
    bool m_waitList;
    int m_conflict;
    uint64_t m_walkSeen;
    QString m_walkLocal;
    QString m_walkRemote;
    QList<QString> m_walked;
    QList<Job> m_queue;

    QLineEdit* m_localPath;
    QLineEdit* m_remotePath;
    QTableWidget* m_local;
    QTableWidget* m_remote;
    QPushButton* m_toRemote;
    QPushButton* m_toLocal;
    QLabel* m_status;
    QProgressBar* m_progress;
    QTimer* m_timer;
};

}

#endif
