/**
 * @file FileTransferDialog.cpp
 * @brief 双栏文件传输
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "FileTransferDialog.h"
#include "AuditLog.h"
#include "Messages.h"
#include "RemoteTheme.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

namespace Vision {

FileTransferDialog::FileTransferDialog(ControlSession* session, AuditLog* audit, const std::string& peerId,
    const QString& deviceName, const QString& deviceId, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_audit(audit)
    , m_peerId(peerId)
    , m_name(deviceName)
    , m_deviceId(deviceId)
    , m_dirSeen(0)
    , m_waitSeq(0)
    , m_nextId(static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff))
    , m_waiting(false)
    , m_waitUpload(false)
    , m_waitList(false)
    , m_conflict(0)
    , m_walkSeen(0)
    , m_localPath(0)
    , m_remotePath(0)
    , m_local(0)
    , m_remote(0)
    , m_toRemote(0)
    , m_toLocal(0)
    , m_status(0)
    , m_progress(0)
    , m_timer(new QTimer(this))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(QString::fromUtf8("文件传输"));
    resize(980, 560);
    build();
    reloadLocal();
    requestRemote();
    QObject::connect(m_timer, &QTimer::timeout, [this]() { tick(); });
    m_timer->start(200);
}

void FileTransferDialog::build()
{
    QVBoxLayout* left = new QVBoxLayout();
    QVBoxLayout* right = new QVBoxLayout();
    left->setSpacing(6);
    right->setSpacing(6);
    QLabel* localTitle = new QLabel(QString::fromUtf8("本机"), this);
    QLabel* remoteTitle = new QLabel(QString::fromUtf8("被控端 · %1").arg(m_name), this);
    localTitle->setStyleSheet("font-weight:600;");
    remoteTitle->setStyleSheet("font-weight:600;");
    m_localPath = new QLineEdit(this);
    m_remotePath = new QLineEdit(this);
    m_localPath->setPlaceholderText(QString::fromUtf8("此电脑"));
    m_remotePath->setPlaceholderText(QString::fromUtf8("允许的根目录"));
    QPushButton* localUp = new QPushButton(QString::fromUtf8("上级"), this);
    QPushButton* localRefresh = new QPushButton(QString::fromUtf8("刷新"), this);
    QPushButton* remoteUp = new QPushButton(QString::fromUtf8("上级"), this);
    QPushButton* remoteRefresh = new QPushButton(QString::fromUtf8("刷新"), this);
    QHBoxLayout* localBar = new QHBoxLayout();
    QHBoxLayout* remoteBar = new QHBoxLayout();
    localBar->addWidget(m_localPath, 1);
    localBar->addWidget(localUp);
    localBar->addWidget(localRefresh);
    remoteBar->addWidget(m_remotePath, 1);
    remoteBar->addWidget(remoteUp);
    remoteBar->addWidget(remoteRefresh);

    m_local = new QTableWidget(0, 3, this);
    m_remote = new QTableWidget(0, 3, this);
    const QStringList headers = QStringList()
        << QString::fromUtf8("名称") << QString::fromUtf8("大小") << QString::fromUtf8("修改时间");
    for (int side = 0; side < 2; ++side) {
        QTableWidget* table = side == 0 ? m_local : m_remote;
        table->setHorizontalHeaderLabels(headers);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setShowGrid(false);
        table->setAlternatingRowColors(true);
        table->setStyleSheet("QTableWidget{border:1px solid #F0F0F0;}");
    }

    left->addWidget(localTitle);
    left->addLayout(localBar);
    left->addWidget(m_local, 1);
    right->addWidget(remoteTitle);
    right->addLayout(remoteBar);
    right->addWidget(m_remote, 1);

    m_toRemote = RemoteTheme::button(QString::fromUtf8("传到被控端"), "toRight", "primary", this);
    m_toLocal = RemoteTheme::button(QString::fromUtf8("传到本机"), "toLeft", 0, this);
    m_toRemote->setEnabled(false);
    m_toLocal->setEnabled(false);
    QVBoxLayout* mid = new QVBoxLayout();
    mid->addStretch(1);
    mid->addWidget(m_toRemote);
    mid->addWidget(m_toLocal);
    mid->addStretch(1);

    QHBoxLayout* panes = new QHBoxLayout();
    panes->setSpacing(10);
    panes->addLayout(left, 1);
    panes->addLayout(mid);
    panes->addLayout(right, 1);

    m_status = new QLabel(QString::fromUtf8("双击文件夹进入。选中文件或文件夹后，可以整夹传到另一边。"), this);
    m_status->setWordWrap(true);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    QPushButton* close = new QPushButton(QString::fromUtf8("关闭"), this);
    QHBoxLayout* foot = new QHBoxLayout();
    foot->addWidget(m_status, 1);
    foot->addWidget(close);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->addLayout(panes, 1);
    root->addWidget(m_progress);
    root->addLayout(foot);

    QObject::connect(localUp, &QPushButton::clicked, [this]() { goUp(true); });
    QObject::connect(remoteUp, &QPushButton::clicked, [this]() { goUp(false); });
    QObject::connect(localRefresh, &QPushButton::clicked, [this]() { reloadLocal(); });
    QObject::connect(remoteRefresh, &QPushButton::clicked, [this]() { requestRemote(); });
    QObject::connect(m_localPath, &QLineEdit::returnPressed, [this]() { commitPath(true); });
    QObject::connect(m_remotePath, &QLineEdit::returnPressed, [this]() { commitPath(false); });
    QObject::connect(m_local, &QTableWidget::itemDoubleClicked, [this](QTableWidgetItem* item) {
        if (item) {
            enterRow(true, item->row());
        }
    });
    QObject::connect(m_remote, &QTableWidget::itemDoubleClicked, [this](QTableWidgetItem* item) {
        if (item) {
            enterRow(false, item->row());
        }
    });
    QObject::connect(m_local, &QTableWidget::itemSelectionChanged, [this]() { syncButtons(); });
    QObject::connect(m_remote, &QTableWidget::itemSelectionChanged, [this]() { syncButtons(); });
    QObject::connect(m_toRemote, &QPushButton::clicked, [this]() { uploadSelected(); });
    QObject::connect(m_toLocal, &QPushButton::clicked, [this]() { downloadSelected(); });
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void FileTransferDialog::addRow(QTableWidget* table, const QString& kind, const QString& name, quint64 size, const QString& time)
{
    const int row = table->rowCount();
    table->insertRow(row);
    const QString shown = kind == "R"
        ? QString::fromUtf8("［链接］ ") + name
        : (kind == "D" ? QString::fromUtf8("［文件夹］ ") + name : name);
    QTableWidgetItem* nameItem = new QTableWidgetItem(shown);
    nameItem->setData(Qt::UserRole, kind);
    nameItem->setData(Qt::UserRole + 1, name);
    nameItem->setData(Qt::UserRole + 2, size);
    QTableWidgetItem* sizeItem = new QTableWidgetItem(kind == "F" ? sizeText(size) : QString());
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 0, nameItem);
    table->setItem(row, 1, sizeItem);
    table->setItem(row, 2, new QTableWidgetItem(time));
}

void FileTransferDialog::syncButtons()
{
    bool picked = false;
    const QModelIndexList rows = m_local->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        QTableWidgetItem* item = m_local->item(rows.at(i).row(), 0);
        const QString kind = item ? item->data(Qt::UserRole).toString() : QString();
        if (kind == "F" || kind == "D" || kind == "R") {
            picked = true;
        }
    }
    m_toRemote->setEnabled(picked && !m_remoteDir.isEmpty());
    picked = false;
    const QModelIndexList remoteRows = m_remote->selectionModel()->selectedRows();
    for (int i = 0; i < remoteRows.size(); ++i) {
        QTableWidgetItem* item = m_remote->item(remoteRows.at(i).row(), 0);
        const QString kind = item ? item->data(Qt::UserRole).toString() : QString();
        if (kind == "F" || kind == "D" || kind == "R") {
            picked = true;
        }
    }
    m_toLocal->setEnabled(picked && !m_localDir.isEmpty());
}

QString FileTransferDialog::childPath(const QString& dir, const QString& name) const
{
    QString next = dir;
    next.replace('/', '\\');
    if (!next.isEmpty() && !next.endsWith('\\')) {
        next += '\\';
    }
    next += name;
    return next;
}

QString FileTransferDialog::sizeText(quint64 n) const
{
    if (n < 1024) {
        return QString::fromUtf8("%1 B").arg(n);
    }
    if (n < 1024ull * 1024ull) {
        return QString::fromUtf8("%1 KB").arg(QString::number(n / 1024.0, 'f', n < 10240 ? 1 : 0));
    }
    if (n < 1024ull * 1024ull * 1024ull) {
        return QString::fromUtf8("%1 MB").arg(QString::number(n / (1024.0 * 1024.0), 'f', 1));
    }
    return QString::fromUtf8("%1 GB").arg(QString::number(n / (1024.0 * 1024.0 * 1024.0), 'f', 2));
}

void FileTransferDialog::reloadLocal()
{
    m_local->setRowCount(0);
    m_localPath->setText(m_localDir.isEmpty() ? QString() : QDir::toNativeSeparators(m_localDir));
    if (m_localDir.isEmpty()) {
        const QFileInfoList drives = QDir::drives();
        for (int i = 0; i < drives.size(); ++i) {
            addRow(m_local, "D", QDir::toNativeSeparators(drives.at(i).absoluteFilePath()), 0, QString());
        }
        syncButtons();
        return;
    }
    QDir dir(m_localDir);
    if (!dir.exists()) {
        m_status->setText(QString::fromUtf8("本机路径不存在"));
        syncButtons();
        return;
    }
    struct Row {
        QString kind;
        QString name;
        quint64 size;
        QString time;
    };
    QVector<Row> rows;
    const QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (int i = 0; i < list.size(); ++i) {
        Row row;
        row.kind = list.at(i).isDir() ? "D" : "F";
        row.name = list.at(i).fileName();
        row.size = list.at(i).isDir() ? 0 : static_cast<quint64>(list.at(i).size());
        row.time = list.at(i).lastModified().toString("yyyy-MM-dd HH:mm");
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.kind != b.kind) {
            return a.kind < b.kind;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    for (int i = 0; i < rows.size(); ++i) {
        addRow(m_local, rows.at(i).kind, rows.at(i).name, rows.at(i).size, rows.at(i).time);
    }
    syncButtons();
}

void FileTransferDialog::requestRemote()
{
    if (m_waitList) {
        return;
    }
    std::string err;
    m_session->post(m_peerId, Messages::packListDir(m_remoteDir.toUtf8().constData(), &err));
}

void FileTransferDialog::fillRemote(const QString& listing)
{
    m_remote->setRowCount(0);
    m_remotePath->setText(m_remoteDir);
    struct Row {
        QString kind;
        QString name;
        quint64 size;
        QString time;
    };
    QVector<Row> rows;
    const QStringList lines = listing.split('\n', Qt::SkipEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        const QStringList parts = lines.at(i).split('\t');
        if (parts.size() < 3) {
            continue;
        }
        if (parts.at(0) == "E") {
            m_status->setText(parts.at(2));
            continue;
        }
        if (parts.at(0) != "D" && parts.at(0) != "F" && parts.at(0) != "R") {
            continue;
        }
        Row row;
        row.kind = parts.at(0);
        row.size = parts.at(1).toULongLong();
        row.name = parts.at(2);
        row.time = parts.size() >= 4 ? parts.at(3) : QString();
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        const int ra = a.kind == "F" ? 1 : 0;
        const int rb = b.kind == "F" ? 1 : 0;
        if (ra != rb) {
            return ra < rb;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    for (int i = 0; i < rows.size(); ++i) {
        addRow(m_remote, rows.at(i).kind, rows.at(i).name, rows.at(i).size, rows.at(i).time);
    }
    syncButtons();
}

void FileTransferDialog::enterRow(bool localSide, int row)
{
    QTableWidget* table = localSide ? m_local : m_remote;
    QTableWidgetItem* item = table->item(row, 0);
    if (!item) {
        return;
    }
    const QString kind = item->data(Qt::UserRole).toString();
    if (kind != "D" && kind != "R") {
        return;
    }
    const QString name = item->data(Qt::UserRole + 1).toString();
    if (localSide) {
        m_localDir = m_localDir.isEmpty() ? name : childPath(m_localDir, name);
        if (!m_localDir.endsWith('\\')) {
            m_localDir += '\\';
        }
        reloadLocal();
        return;
    }
    m_remoteDir = m_remoteDir.isEmpty() ? name : childPath(m_remoteDir, name);
    if (!m_remoteDir.endsWith('\\')) {
        m_remoteDir += '\\';
    }
    requestRemote();
}

void FileTransferDialog::goUp(bool localSide)
{
    QString text = localSide ? m_localDir : m_remoteDir;
    text.replace('/', '\\');
    if (text.endsWith('\\')) {
        text.chop(1);
    }
    QString parent;
    if (!text.isEmpty() && text.size() > 2) {
        const int slash = text.lastIndexOf('\\');
        parent = slash < 0 ? QString() : text.left(slash + 1);
    }
    if (localSide) {
        m_localDir = parent;
        reloadLocal();
        return;
    }
    m_remoteDir = parent;
    requestRemote();
}

void FileTransferDialog::commitPath(bool localSide)
{
    QLineEdit* edit = localSide ? m_localPath : m_remotePath;
    QString text = edit->text().trimmed();
    if (text == QString::fromUtf8("此电脑")) {
        text.clear();
    }
    text.replace('/', '\\');
    if (localSide) {
        if (!text.isEmpty() && !QFileInfo(text).isDir()) {
            m_status->setText(QString::fromUtf8("本机路径不存在"));
            m_localPath->setText(m_localDir);
            return;
        }
        if (!text.isEmpty() && !text.endsWith('\\')) {
            text += '\\';
        }
        m_localDir = text;
        reloadLocal();
        return;
    }
    if (!text.isEmpty() && !text.endsWith('\\')) {
        text += '\\';
    }
    m_remoteDir = text;
    requestRemote();
}

bool FileTransferDialog::nameTaken(bool remoteSide, const QString& full) const
{
    if (!remoteSide && QFileInfo::exists(full)) {
        return true;
    }
    QString parent = full;
    parent.replace('/', '\\');
    while (parent.endsWith('\\')) {
        parent.chop(1);
    }
    const int slash = parent.lastIndexOf('\\');
    const QString leaf = slash < 0 ? parent : parent.mid(slash + 1);
    parent = slash < 0 ? QString() : parent.left(slash + 1);
    QString view = remoteSide ? m_remoteDir : m_localDir;
    view.replace('/', '\\');
    if (!view.isEmpty() && !view.endsWith('\\')) {
        view += '\\';
    }
    if (remoteSide && parent.compare(view, Qt::CaseInsensitive) == 0) {
        for (int i = 0; i < m_remote->rowCount(); ++i) {
            QTableWidgetItem* item = m_remote->item(i, 0);
            if (item && item->data(Qt::UserRole + 1).toString().compare(leaf, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        const QString queued = remoteSide ? m_queue.at(i).remote : m_queue.at(i).local;
        if (queued.compare(full, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString FileTransferDialog::indexed(const QString& path, int n) const
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().isEmpty() ? QString() : QString(".") + info.suffix();
    return QDir::toNativeSeparators(info.dir().filePath(info.completeBaseName() + QString(" (%1)").arg(n) + suffix));
}

QString FileTransferDialog::choosePath(const QString& path, bool remoteSide, quint64 have, quint64 total, uint64_t* resume)
{
    if (resume) {
        *resume = 0;
    }
    if (!nameTaken(remoteSide, path)) {
        return path;
    }
    if (m_conflict == 1) {
        if (!remoteSide) {
            QFile::remove(path);
        }
        return path;
    }
    if (m_conflict == 2) {
        return QString();
    }
    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("文件已存在"));
    box.setText(QString::fromUtf8("%1 已存在").arg(path));
    QPushButton* overwrite = box.addButton(QString::fromUtf8("覆盖"), QMessageBox::AcceptRole);
    QPushButton* overwriteAll = box.addButton(QString::fromUtf8("全部覆盖"), QMessageBox::AcceptRole);
    QPushButton* rename = box.addButton(QString::fromUtf8("重命名"), QMessageBox::ActionRole);
    QPushButton* cont = 0;
    if (!remoteSide && have > 0 && (total == 0 || have < total)) {
        cont = box.addButton(QString::fromUtf8("续传"), QMessageBox::ActionRole);
    }
    QPushButton* skipAll = box.addButton(QString::fromUtf8("全部跳过"), QMessageBox::RejectRole);
    box.addButton(QString::fromUtf8("跳过"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == overwriteAll) {
        m_conflict = 1;
    }
    if (box.clickedButton() == skipAll) {
        m_conflict = 2;
        return QString();
    }
    if (box.clickedButton() == overwrite || box.clickedButton() == overwriteAll) {
        if (!remoteSide) {
            QFile::remove(path);
        }
        return path;
    }
    if (cont && box.clickedButton() == cont) {
        if (resume) {
            *resume = have;
        }
        return path;
    }
    if (box.clickedButton() != rename) {
        return QString();
    }
    for (int n = 1; n < 1000; ++n) {
        const QString next = indexed(path, n);
        if (!nameTaken(remoteSide, next)) {
            return next;
        }
    }
    return QString();
}

QString FileTransferDialog::folderName(const QString& path) const
{
    QString text = path;
    text.replace('/', '\\');
    while (text.endsWith('\\')) {
        text.chop(1);
    }
    const int slash = text.lastIndexOf('\\');
    QString leaf = slash < 0 ? text : text.mid(slash + 1);
    if (leaf.endsWith(':')) {
        leaf.chop(1);
    }
    if (leaf.isEmpty()) {
        leaf = QString::fromUtf8("文件夹");
    }
    return leaf;
}

QString FileTransferDialog::itemPath(bool localSide, const QString& name) const
{
    const QString dir = localSide ? m_localDir : m_remoteDir;
    if (dir.isEmpty()) {
        return QDir::toNativeSeparators(name);
    }
    return childPath(dir, name);
}

void FileTransferDialog::enqueueUploadDir(const QString& local, const QString& remote)
{
    Job made;
    made.kind = KindMkdir;
    made.local = local;
    made.remote = remote;
    made.resume = 0;
    made.bytes = 0;
    m_queue.append(made);
    const QFileInfoList list = QDir(local).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).isSymLink()) {
            continue;
        }
        const QString nextRemote = childPath(remote, list.at(i).fileName());
        if (list.at(i).isDir()) {
            enqueueUploadDir(QDir::toNativeSeparators(list.at(i).absoluteFilePath()), nextRemote);
        } else {
            enqueueUploadFile(QDir::toNativeSeparators(list.at(i).absoluteFilePath()), nextRemote,
                static_cast<quint64>(list.at(i).size()));
        }
    }
}

void FileTransferDialog::enqueueUploadFile(const QString& local, const QString& remote, quint64 bytes)
{
    uint64_t resume = 0;
    const QString chosen = choosePath(remote, true, 0, 0, &resume);
    if (chosen.isEmpty()) {
        return;
    }
    Job job;
    job.kind = KindUpload;
    job.local = local;
    job.remote = chosen;
    job.resume = 0;
    job.bytes = bytes;
    m_queue.append(job);
}

void FileTransferDialog::enqueueDownloadDir(const QString& remote, const QString& local)
{
    for (int i = 0; i < m_walked.size(); ++i) {
        if (m_walked.at(i).compare(remote, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    m_walked.append(remote);
    QDir().mkpath(local);
    Job job;
    job.kind = KindList;
    job.local = local;
    job.remote = remote;
    job.resume = 0;
    job.bytes = 0;
    m_queue.append(job);
}

void FileTransferDialog::enqueueDownloadFile(const QString& remote, const QString& local, quint64 bytes)
{
    const quint64 have = QFileInfo::exists(local) ? static_cast<quint64>(QFileInfo(local).size()) : 0;
    uint64_t resume = 0;
    const QString chosen = choosePath(local, false, have, bytes, &resume);
    if (chosen.isEmpty()) {
        return;
    }
    Job job;
    job.kind = KindDownload;
    job.local = chosen;
    job.remote = remote;
    job.resume = resume;
    job.bytes = bytes;
    m_queue.append(job);
}

void FileTransferDialog::uploadSelected()
{
    if (m_remoteDir.isEmpty()) {
        m_status->setText(QString::fromUtf8("先进入被控端的一个文件夹"));
        return;
    }
    m_conflict = 0;
    const int before = m_queue.size();
    const QModelIndexList rows = m_local->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        QTableWidgetItem* item = m_local->item(rows.at(i).row(), 0);
        if (!item) {
            continue;
        }
        const QString kind = item->data(Qt::UserRole).toString();
        const QString name = item->data(Qt::UserRole + 1).toString();
        const QString local = itemPath(true, name);
        if (kind == "D") {
            enqueueUploadDir(local, childPath(m_remoteDir, folderName(name)));
        } else if (kind == "F") {
            enqueueUploadFile(local, childPath(m_remoteDir, name), item->data(Qt::UserRole + 2).toULongLong());
        }
    }
    if (m_queue.size() == before && !m_waiting) {
        m_status->setText(QString::fromUtf8("没有要传的文件"));
        return;
    }
    pump();
}

void FileTransferDialog::downloadSelected()
{
    if (m_localDir.isEmpty()) {
        m_status->setText(QString::fromUtf8("先进入本机的一个文件夹"));
        return;
    }
    m_conflict = 0;
    m_walked.clear();
    const int before = m_queue.size();
    const QModelIndexList rows = m_remote->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        QTableWidgetItem* item = m_remote->item(rows.at(i).row(), 0);
        if (!item) {
            continue;
        }
        const QString kind = item->data(Qt::UserRole).toString();
        const QString name = item->data(Qt::UserRole + 1).toString();
        const QString remote = itemPath(false, name);
        if (kind == "D" || kind == "R") {
            enqueueDownloadDir(remote, childPath(m_localDir, folderName(name)));
        } else if (kind == "F") {
            enqueueDownloadFile(remote, childPath(m_localDir, name), item->data(Qt::UserRole + 2).toULongLong());
        }
    }
    if (m_queue.size() == before && !m_waiting) {
        m_status->setText(QString::fromUtf8("没有要传的文件"));
        return;
    }
    pump();
}

void FileTransferDialog::takeWalk(const QString& listing)
{
    m_waitList = false;
    m_waiting = false;
    const QStringList lines = listing.split('\n', Qt::SkipEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        const QStringList parts = lines.at(i).split('\t');
        if (parts.size() < 3) {
            continue;
        }
        if (parts.at(0) == "E") {
            m_status->setText(parts.at(2));
            continue;
        }
        const QString name = parts.at(2);
        if (parts.at(0) == "D") {
            enqueueDownloadDir(childPath(m_walkRemote, name), childPath(m_walkLocal, name));
        } else if (parts.at(0) == "F") {
            enqueueDownloadFile(childPath(m_walkRemote, name), childPath(m_walkLocal, name), parts.at(1).toULongLong());
        }
    }
    pump();
    finishBatchView();
}

void FileTransferDialog::finishBatchView()
{
    if (m_waiting || m_waitList || !m_queue.isEmpty()) {
        return;
    }
    reloadLocal();
    requestRemote();
}

bool FileTransferDialog::copyTile(ControlSession::Tile* out) const
{
    if (!out) {
        return false;
    }
    const std::vector<ControlSession::Tile> tiles = m_session->tiles();
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (tiles[i].peerId == m_peerId) {
            *out = tiles[i];
            return true;
        }
    }
    return false;
}

void FileTransferDialog::pump()
{
    if (m_waiting || m_queue.isEmpty()) {
        return;
    }
    ControlSession::Tile tile;
    if (!copyTile(&tile) || !tile.online || tile.peerId.empty()) {
        m_status->setText(QString::fromUtf8("被控端不在线"));
        return;
    }
    const Job job = m_queue.takeFirst();
    const uint32_t id = ++m_nextId;
    std::string err;
    const int left = m_queue.size();
    const QString tail = left > 0 ? QString::fromUtf8("，还有 %1 个").arg(left) : QString();
    if (job.kind == KindList) {
        m_walkLocal = job.local;
        m_walkRemote = job.remote;
        m_walkSeen = tile.dirSeq;
        m_dirSeen = tile.dirSeq;
        m_waitList = true;
        m_waiting = true;
        m_session->post(m_peerId, Messages::packListDir(job.remote.toUtf8().constData(), &err));
        m_status->setText(QString::fromUtf8("正在读取 %1%2").arg(folderName(job.remote), tail));
        return;
    }
    m_waitSeq = tile.fileSeq;
    m_progress->setValue(0);
    if (job.kind == KindMkdir) {
        m_waiting = true;
        m_waitUpload = true;
        m_session->post(m_peerId, Messages::packMkDir(id, job.remote.toUtf8().constData(), &err));
        m_status->setText(QString::fromUtf8("正在建目录 %1%2").arg(folderName(job.remote), tail));
        return;
    }
    if (job.kind == KindUpload) {
        const std::string sha = m_session->armUpload(m_peerId, id, job.local.toUtf8().constData(), job.remote.toUtf8().constData());
        if (sha.empty()) {
            m_status->setText(QString::fromUtf8("打不开本地文件"));
            pump();
            return;
        }
        m_waiting = true;
        m_waitUpload = true;
        m_session->post(m_peerId, Messages::packFileOpen(id, 0, job.remote.toUtf8().constData(), job.bytes, &err, sha, 0));
        m_audit->add("file.upload", m_name, m_deviceId, job.remote, job.local);
        m_status->setText(QString::fromUtf8("正在上传 %1%2").arg(QFileInfo(job.local).fileName(), tail));
        return;
    }
    m_waiting = true;
    m_waitUpload = false;
    m_session->armDownload(m_peerId, id, job.local.toUtf8().constData(), job.bytes);
    m_session->post(m_peerId, Messages::packFileOpen(id, 1, job.remote.toUtf8().constData(), 0, &err, std::string(), job.resume));
    m_audit->add("file.download", m_name, m_deviceId, job.remote, job.local);
    m_status->setText(QString::fromUtf8("正在下载 %1%2").arg(QFileInfo(job.remote).fileName(), tail));
}

void FileTransferDialog::tick()
{
    ControlSession::Tile tile;
    if (!copyTile(&tile)) {
        return;
    }
    if (tile.dirSeq != m_dirSeen) {
        m_dirSeen = tile.dirSeq;
        if (m_waitList && tile.dirSeq != m_walkSeen) {
            takeWalk(QString::fromUtf8(tile.dirList.c_str()));
            return;
        }
        if (!m_waitList) {
            fillRemote(QString::fromUtf8(tile.dirList.c_str()));
        }
    }
    if (m_waitList) {
        return;
    }
    if (m_waiting) {
        if (tile.fileBytes > 0) {
            const int value = static_cast<int>((tile.fileGot * 1000ull) / tile.fileBytes);
            m_progress->setValue(value > 1000 ? 1000 : value);
        }
        const QString note = QString::fromUtf8(tile.fileNote.c_str());
        if (tile.fileBytes > 0 && (note.contains(QString::fromUtf8("正在")) || note.contains(QString::fromUtf8("等待")))) {
            m_status->setText(note + QString("  %1 / %2").arg(sizeText(tile.fileGot), sizeText(tile.fileBytes))
                + (m_queue.isEmpty() ? QString() : QString::fromUtf8("，还有 %1 个").arg(m_queue.size())));
        }
        if (tile.fileSeq != m_waitSeq) {
            m_waiting = false;
            m_status->setText(note);
            m_progress->setValue(note == QString::fromUtf8("完成") ? 1000 : m_progress->value());
            pump();
            finishBatchView();
        }
        return;
    }
    pump();
}

}
