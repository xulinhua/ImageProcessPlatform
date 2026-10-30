/**
 * @file SessionPhase4.cpp
 * @brief 会话窗口第四期：剪贴板、文件、聊天、电源、隐私屏、录屏
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SessionWindow.h"
#include "FrameView.h"
#include "AuditLog.h"
#include "ControlConfig.h"
#include "RemoteFiles.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <cstring>

namespace Vision {

void SessionWindow::inviteViewer()
{
    if (!m_online || !m_tile.operating) {
        flash(QString::fromUtf8("先取得操作权，再邀请其他中控台"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("邀请观看"));
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QLineEdit* host = new QLineEdit(&dialog);
    host->setPlaceholderText(QString::fromUtf8("另一台中控台的 IP"));
    QPushButton* ok = new QPushButton(QString::fromUtf8("邀请"), &dialog);
    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->addWidget(new QLabel(QString::fromUtf8("对方中控台需已在监听。被控端会再连过去，默认只看。"), &dialog));
    root->addWidget(host);
    root->addWidget(ok);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted || host->text().trimmed().isEmpty()) {
        return;
    }
    std::string err;
    m_session->post(m_tile.peerId, Messages::packInvite(host->text().trimmed().toUtf8().constData(),
        static_cast<uint16_t>(RemoteConst::kPort), &err));
    addEvent(QString::fromUtf8("邀请 %1 观看").arg(host->text().trimmed()));
}

void SessionWindow::pushClipboard()
{
    if (!m_online || !m_tile.operating) {
        flash(QString::fromUtf8("先取得操作权，再同步剪贴板"));
        return;
    }
    std::string err;
    const QString text = QApplication::clipboard()->text();
    if (!text.isEmpty()) {
        m_session->post(m_tile.peerId, Messages::packClipText(text.toUtf8().constData(), &err));
        addEvent(QString::fromUtf8("已发送剪贴板文本"));
    }
    const QImage image = QApplication::clipboard()->image();
    if (!image.isNull()) {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG") && bytes.size() <= 512 * 1024) {
            CommBytes png(bytes.constData(), bytes.constData() + bytes.size());
            m_session->post(m_tile.peerId, Messages::packClipImage(png, &err));
            addEvent(QString::fromUtf8("已发送剪贴板图片"));
        }
    }
    if (text.isEmpty() && image.isNull()) {
        flash(QString::fromUtf8("剪贴板里没有文本或图片"));
    }
}

void SessionWindow::sendChat()
{
    if (!m_online || !m_chatEdit) {
        return;
    }
    const QString text = m_chatEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    std::string err;
    const QString name = RemoteFiles::userName();
    const int kind = m_chatKind ? m_chatKind->currentData().toInt() : 0;
    m_session->post(m_tile.peerId, Messages::packChat(name.toUtf8().constData(), text.toUtf8().constData(), &err,
        kind == 1 ? 1 : 0));
    addEvent((kind == 1 ? QString::fromUtf8("弹窗 ") : QString()) + name + QString::fromUtf8("：") + text);
    m_chatEdit->clear();
}

void SessionWindow::sendPower(uint8_t action)
{
    if (!m_online || !m_tile.operating) {
        flash(QString::fromUtf8("先取得操作权，再执行电源操作"));
        return;
    }
    const char* names[] = { "", "锁屏", "注销", "重启", "关机" };
    const QString name = QString::fromUtf8(action >= 1 && action <= 4 ? names[action] : "电源");
    if (QMessageBox::question(this, name, QString::fromUtf8("确定对这台设备执行%1？").arg(name)) != QMessageBox::Yes) {
        return;
    }
    std::string err;
    m_session->post(m_tile.peerId, Messages::packPower(action, &err));
    m_audit->add("power", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile), name);
    addEvent(name);
}

void SessionWindow::togglePrivacy()
{
    if (!m_online || !m_tile.operating) {
        flash(QString::fromUtf8("先取得操作权，再开隐私屏"));
        if (m_privacy) {
            m_privacy->setChecked(false);
        }
        return;
    }
    const bool on = m_privacy && m_privacy->isChecked();
    std::string err;
    m_session->post(m_tile.peerId, Messages::packPrivacy(on ? 1 : 0, &err));
    m_audit->add("privacy", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile),
        on ? QString::fromUtf8("开启") : QString::fromUtf8("关闭"));
    addEvent(on ? QString::fromUtf8("隐私屏开") : QString::fromUtf8("隐私屏关"));
}

void SessionWindow::toggleRecord()
{
    if (m_recording) {
        stopRecord();
        return;
    }
    if (!m_online || m_view->image().isNull()) {
        flash(QString::fromUtf8("还没有画面，无法录屏"));
        return;
    }
    const QString dir = m_config->recordDir.isEmpty() ? m_config->shotDir : m_config->recordDir;
    QDir().mkpath(dir);
    const QString stem = QDir(dir).filePath(
        m_name + QString::fromUtf8("-") + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
    const QImage& frame = m_view->image();
    if (m_mp4.start(QDir::toNativeSeparators(stem + QString::fromUtf8(".mp4")).toStdWString(), frame.width(), frame.height(), 5)) {
        m_recording = true;
        if (m_record) {
            m_record->setText(QString::fromUtf8("停止"));
        }
        addEvent(QString::fromUtf8("开始录屏"));
        m_audit->add("record.start", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile), stem + QString::fromUtf8(".mp4"));
        return;
    }
    const QString path = stem + QString::fromUtf8(".avi");
    m_avi = _wfopen(reinterpret_cast<const wchar_t*>(path.utf16()), L"wb");
    if (!m_avi) {
        flash(QString::fromUtf8("录屏文件创建失败"));
        return;
    }
    m_aviW = m_tile.imageW;
    m_aviH = m_tile.imageH;
    m_aviFrames = 0;
    m_aviSizes.clear();
    unsigned char head[224];
    std::memset(head, 0, sizeof(head));
    std::memcpy(head + 0, "RIFF", 4);
    std::memcpy(head + 8, "AVI ", 4);
    std::memcpy(head + 12, "LIST", 4);
    const uint32_t hdrl = 192;
    std::memcpy(head + 16, &hdrl, 4);
    std::memcpy(head + 20, "hdrl", 4);
    std::memcpy(head + 24, "avih", 4);
    const uint32_t avih = 56;
    std::memcpy(head + 28, &avih, 4);
    const uint32_t usec = 200000;
    std::memcpy(head + 32, &usec, 4);
    const uint32_t flags = 0x10;
    std::memcpy(head + 44, &flags, 4);
    const uint32_t streams = 1;
    std::memcpy(head + 56, &streams, 4);
    std::memcpy(head + 64, &m_aviW, 4);
    std::memcpy(head + 68, &m_aviH, 4);
    std::memcpy(head + 88, "LIST", 4);
    const uint32_t strl = 116;
    std::memcpy(head + 92, &strl, 4);
    std::memcpy(head + 96, "strl", 4);
    std::memcpy(head + 100, "strh", 4);
    const uint32_t strh = 56;
    std::memcpy(head + 104, &strh, 4);
    std::memcpy(head + 108, "vids", 4);
    std::memcpy(head + 112, "MJPG", 4);
    const uint32_t scale = 1;
    const uint32_t rate = 5;
    std::memcpy(head + 132, &scale, 4);
    std::memcpy(head + 136, &rate, 4);
    const int quality = -1;
    std::memcpy(head + 156, &quality, 4);
    std::memcpy(head + 168, &m_aviW, 4);
    std::memcpy(head + 172, &m_aviH, 4);
    std::memcpy(head + 176, "strf", 4);
    const uint32_t strf = 40;
    std::memcpy(head + 180, &strf, 4);
    std::memcpy(head + 184, &strf, 4);
    std::memcpy(head + 188, &m_aviW, 4);
    std::memcpy(head + 192, &m_aviH, 4);
    const uint16_t planes = 1;
    const uint16_t bits = 24;
    std::memcpy(head + 196, &planes, 2);
    std::memcpy(head + 198, &bits, 2);
    std::memcpy(head + 200, "MJPG", 4);
    std::memcpy(head + 212, "LIST", 4);
    std::memcpy(head + 220, "movi", 4);
    fwrite(head, 1, sizeof(head), m_avi);
    m_recording = true;
    m_aviLast = 0;
    if (m_record) {
        m_record->setText(QString::fromUtf8("停止"));
    }
    addEvent(QString::fromUtf8("开始录屏"));
    m_audit->add("record.start", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile), path);
}

void SessionWindow::appendRecord(const CommBytes& jpeg, uint32_t width, uint32_t height)
{
    if (m_mp4.active()) {
        m_mp4.write(m_view->image());
        return;
    }
    if (!m_avi || jpeg.empty()) {
        return;
    }
    if (width != m_aviW || height != m_aviH) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_aviLast != 0 && now - m_aviLast < 200) {
        return;
    }
    m_aviLast = now;
    unsigned char chunk[8];
    std::memcpy(chunk, "00dc", 4);
    const uint32_t size = static_cast<uint32_t>(jpeg.size());
    std::memcpy(chunk + 4, &size, 4);
    fwrite(chunk, 1, 8, m_avi);
    fwrite(jpeg.data(), 1, jpeg.size(), m_avi);
    if (jpeg.size() & 1) {
        const unsigned char pad = 0;
        fwrite(&pad, 1, 1, m_avi);
    }
    m_aviSizes.push_back(size);
    m_aviFrames += 1;
}

void SessionWindow::stopRecord()
{
    if (m_mp4.active()) {
        m_mp4.stop();
        m_recording = false;
        if (m_record) {
            m_record->setText(QString::fromUtf8("录屏"));
        }
        addEvent(QString::fromUtf8("录屏已保存"));
        m_audit->add("record.stop", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile), QString());
        return;
    }
    if (!m_recording || !m_avi) {
        m_recording = false;
        if (m_record) {
            m_record->setText(QString::fromUtf8("录屏"));
        }
        return;
    }
    uint32_t movi = 4;
    for (size_t i = 0; i < m_aviSizes.size(); ++i) {
        movi += 8 + m_aviSizes[i] + (m_aviSizes[i] & 1);
    }
    fwrite("idx1", 1, 4, m_avi);
    const uint32_t indexBytes = static_cast<uint32_t>(m_aviSizes.size() * 16);
    fwrite(&indexBytes, 4, 1, m_avi);
    uint32_t at = 4;
    for (size_t i = 0; i < m_aviSizes.size(); ++i) {
        fwrite("00dc", 1, 4, m_avi);
        const uint32_t key = 0x10;
        fwrite(&key, 4, 1, m_avi);
        fwrite(&at, 4, 1, m_avi);
        fwrite(&m_aviSizes[i], 4, 1, m_avi);
        at += 8 + m_aviSizes[i] + (m_aviSizes[i] & 1);
    }
    const uint32_t riff = 212 + movi + 8 + indexBytes;
    _fseeki64(m_avi, 4, SEEK_SET);
    fwrite(&riff, 4, 1, m_avi);
    _fseeki64(m_avi, 48, SEEK_SET);
    fwrite(&m_aviFrames, 4, 1, m_avi);
    _fseeki64(m_avi, 140, SEEK_SET);
    fwrite(&m_aviFrames, 4, 1, m_avi);
    _fseeki64(m_avi, 216, SEEK_SET);
    fwrite(&movi, 4, 1, m_avi);
    fclose(m_avi);
    m_avi = 0;
    m_recording = false;
    if (m_record) {
        m_record->setText(QString::fromUtf8("录屏"));
    }
    addEvent(QString::fromUtf8("录屏已保存，%1 帧").arg(m_aviFrames));
    m_audit->add("record.stop", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), shareText(m_tile),
        QString::fromUtf8("%1 帧").arg(m_aviFrames));
}

void SessionWindow::openFiles()
{
    if (!m_online || !m_tile.operating) {
        flash(QString::fromUtf8("先取得操作权，再传文件"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog.setWindowTitle(QString::fromUtf8("文件传输"));
    dialog.resize(520, 420);
    QLineEdit* path = new QLineEdit(&dialog);
    path->setPlaceholderText(QString::fromUtf8("空路径列出盘符，例如 C:\\"));
    QListWidget* list = new QListWidget(&dialog);
    QLabel* note = new QLabel(&dialog);
    QPushButton* up = new QPushButton(QString::fromUtf8("上级"), &dialog);
    QPushButton* refresh = new QPushButton(QString::fromUtf8("刷新"), &dialog);
    QPushButton* download = new QPushButton(QString::fromUtf8("下载"), &dialog);
    QPushButton* upload = new QPushButton(QString::fromUtf8("上传"), &dialog);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(up);
    buttons->addWidget(refresh);
    buttons->addStretch(1);
    buttons->addWidget(download);
    buttons->addWidget(upload);
    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->addWidget(path);
    root->addWidget(list, 1);
    root->addWidget(note);
    root->addLayout(buttons);

    uint64_t seen = 0;
    auto request = [this, path]() {
        std::string err;
        m_session->post(m_tile.peerId, Messages::packListDir(path->text().toUtf8().constData(), &err));
    };
    auto fill = [this, list, note, &seen]() {
        const std::vector<ControlSession::Tile> tiles = m_session->tiles();
        for (size_t i = 0; i < tiles.size(); ++i) {
            if (tiles[i].peerId != m_tile.peerId) {
                continue;
            }
            note->setText(QString::fromUtf8(tiles[i].fileNote.c_str()));
            if (tiles[i].fileBytes > 0 || tiles[i].fileGot > 0) {
                note->setText(note->text() + QString("  %1 / %2 KB")
                    .arg(tiles[i].fileGot / 1024).arg(tiles[i].fileBytes > 0 ? tiles[i].fileBytes / 1024 : tiles[i].fileGot / 1024));
            }
            if (tiles[i].dirSeq == seen) {
                return;
            }
            seen = tiles[i].dirSeq;
            list->clear();
            const QString listing = QString::fromUtf8(tiles[i].dirList.c_str());
            const QStringList rows = listing.split('\n', QString::SkipEmptyParts);
            for (int r = 0; r < rows.size(); ++r) {
                list->addItem(rows.at(r));
            }
        }
    };
    QObject::connect(refresh, &QPushButton::clicked, request);
    QObject::connect(path, &QLineEdit::returnPressed, request);
    QObject::connect(up, &QPushButton::clicked, [path, request]() {
        QString text = path->text();
        text.replace('/', '\\');
        if (text.size() <= 3) {
            path->clear();
        } else {
            if (text.endsWith('\\')) {
                text.chop(1);
            }
            const int slash = text.lastIndexOf('\\');
            path->setText(slash < 0 ? QString() : text.left(slash + 1));
        }
        request();
    });
    QObject::connect(list, &QListWidget::itemDoubleClicked, [path, request](QListWidgetItem* item) {
        const QStringList parts = item->text().split('\t');
        if (parts.size() < 3 || parts.at(0) != "D") {
            return;
        }
        QString next = path->text();
        next.replace('/', '\\');
        if (!next.isEmpty() && !next.endsWith('\\')) {
            next += '\\';
        }
        next += parts.at(2);
        if (!next.endsWith('\\')) {
            next += '\\';
        }
        path->setText(next);
        request();
    });
    QObject::connect(download, &QPushButton::clicked, [this, path, list]() {
        QListWidgetItem* item = list->currentItem();
        if (!item) {
            return;
        }
        const QStringList parts = item->text().split('\t');
        if (parts.size() < 3 || parts.at(0) != "F") {
            return;
        }
        QFileDialog dialog(this, QString::fromUtf8("保存到本机"), parts.at(2));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setOption(QFileDialog::DontConfirmOverwrite, true);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
            return;
        }
        const QString local = resolveConflict(dialog.selectedFiles().at(0), QFileInfo(dialog.selectedFiles().at(0)).exists());
        if (local.isEmpty()) {
            return;
        }
        QString remote = path->text();
        remote.replace('/', '\\');
        if (!remote.endsWith('\\')) {
            remote += '\\';
        }
        remote += parts.at(2);
        const uint32_t id = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff);
        const QFileInfo existing(local);
        const uint64_t resume = existing.exists() ? static_cast<uint64_t>(existing.size()) : 0;
        m_session->armDownload(m_tile.peerId, id, local.toUtf8().constData());
        std::string err;
        m_session->post(m_tile.peerId, Messages::packFileOpen(id, 1, remote.toUtf8().constData(), 0, &err, std::string(), resume));
        m_audit->add("file.download", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), remote, local);
    });
    QObject::connect(upload, &QPushButton::clicked, [this, path, list]() {
        const QString local = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择要上传的文件"));
        if (local.isEmpty()) {
            return;
        }
        QString remote = path->text();
        remote.replace('/', '\\');
        if (remote.isEmpty()) {
            return;
        }
        if (!remote.endsWith('\\')) {
            remote += '\\';
        }
        remote += QFileInfo(local).fileName();
        bool remoteExists = false;
        for (int i = 0; i < list->count(); ++i) {
            const QStringList row = list->item(i)->text().split('\t');
            if (row.size() >= 3 && row.at(0) == "F" && row.at(2) == QFileInfo(local).fileName()) {
                remoteExists = true;
            }
        }
        if (remoteExists) {
            const QString chosen = resolveConflict(remote, true);
            if (chosen.isEmpty()) {
                return;
            }
            remote = chosen;
        }
        const uint32_t id = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff);
        const QFileInfo info(local);
        const std::string sha = m_session->armUpload(m_tile.peerId, id, local.toUtf8().constData(), remote.toUtf8().constData());
        std::string err;
        m_session->post(m_tile.peerId, Messages::packFileOpen(id, 0, remote.toUtf8().constData(),
            static_cast<uint64_t>(info.size()), &err, sha, 0));
        m_audit->add("file.upload", m_name, QString::fromUtf8(m_tile.deviceId.c_str()), remote, local);
    });
    QTimer timer(&dialog);
    QObject::connect(&timer, &QTimer::timeout, fill);
    timer.start(200);
    request();
    dialog.exec();
}

QString SessionWindow::resolveConflict(const QString& path, bool exists)
{
    if (!exists && !QFileInfo::exists(path)) {
        return path;
    }
    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("文件已存在"));
    box.setText(QString::fromUtf8("%1 已存在").arg(path));
    QPushButton* overwrite = box.addButton(QString::fromUtf8("覆盖"), QMessageBox::AcceptRole);
    QPushButton* rename = box.addButton(QString::fromUtf8("重命名"), QMessageBox::ActionRole);
    box.addButton(QString::fromUtf8("跳过"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == overwrite) {
        if (QFileInfo::exists(path)) {
            QFile::remove(path);
        }
        return path;
    }
    if (box.clickedButton() != rename) {
        return QString();
    }
    const QFileInfo info(path);
    for (int n = 1; n < 1000; ++n) {
        const QString suffix = info.suffix().isEmpty() ? QString() : QString(".") + info.suffix();
        const QString next = info.dir().filePath(info.completeBaseName() + QString(" (%1)").arg(n) + suffix);
        if (!QFileInfo::exists(next)) {
            return next;
        }
    }
    return QString();
}

void SessionWindow::pullClipFiles(const QString& listing)
{
    if (!m_online || !m_tile.operating || m_clipPulling) {
        return;
    }
    m_clipQueue = listing.split('\n', QString::SkipEmptyParts);
    m_clipSaved.clear();
    m_clipSawBusy = false;
    advanceClipPull(m_tile);
}

void SessionWindow::advanceClipPull(const ControlSession::Tile& tile)
{
    if (m_clipPulling) {
        const QString note = QString::fromUtf8(tile.fileNote.c_str());
        if (note.contains(QString::fromUtf8("正在"))) {
            m_clipSawBusy = true;
        }
        if (!(m_clipSawBusy && note == QString::fromUtf8("完成"))) {
            return;
        }
        m_clipPulling = false;
        m_clipSawBusy = false;
    }
    if (m_clipQueue.isEmpty()) {
        if (!m_clipSaved.isEmpty()) {
            publishClipFiles();
        }
        return;
    }
    const QString remote = m_clipQueue.takeFirst().trimmed();
    if (remote.isEmpty()) {
        return;
    }
    const QString folder = QDir(QDir::tempPath()).filePath(QString::fromUtf8("RemoteMaster/clip"));
    QDir().mkpath(folder);
    const QString local = QDir(folder).filePath(QFileInfo(remote).fileName());
    QFile::remove(local);
    m_clipSaved.append(QDir::toNativeSeparators(local));
    m_clipPullId = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff);
    m_clipPulling = true;
    m_clipSawBusy = false;
    m_session->armDownload(m_tile.peerId, m_clipPullId, local.toUtf8().constData());
    std::string err;
    m_session->post(m_tile.peerId, Messages::packFileOpen(m_clipPullId, 1, remote.toUtf8().constData(), 0, &err, std::string(), 0));
}

void SessionWindow::publishClipFiles()
{
    QList<QUrl> urls;
    for (int i = 0; i < m_clipSaved.size(); ++i) {
        if (QFileInfo::exists(m_clipSaved.at(i))) {
            urls.append(QUrl::fromLocalFile(m_clipSaved.at(i)));
        }
    }
    m_clipSaved.clear();
    if (urls.isEmpty()) {
        return;
    }
    QMimeData* mime = new QMimeData();
    mime->setUrls(urls);
    QApplication::clipboard()->setMimeData(mime);
    addEvent(QString::fromUtf8("剪贴板文件已保存到本机"));
}

}
