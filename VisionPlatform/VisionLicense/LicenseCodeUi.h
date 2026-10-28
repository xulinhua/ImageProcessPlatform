/**
 * @file LicenseCodeUi.h
 * @brief 申请码、注册码的 txt 导入导出，以及弹出二维码
 */

#ifndef VISION_LICENSE_CODE_UI_H
#define VISION_LICENSE_CODE_UI_H

#include "LicenseUiTheme.h"
#include "qrcodegen.hpp"
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <exception>

inline QString licenseCodeText(const QPlainTextEdit* box)
{
    if (!box) {
        return QString();
    }
    return box->toPlainText().trimmed();
}

inline QString licenseCodeCompact(const QString& code)
{
    QString out;
    out.reserve(code.size());
    for (int i = 0; i < code.size(); ++i) {
        const QChar c = code.at(i);
        if (c == QLatin1Char(' ') || c == QLatin1Char('\r') || c == QLatin1Char('\n') || c == QLatin1Char('\t')) {
            continue;
        }
        out.append(c);
    }
    return out;
}

inline void exportLicenseCode(QWidget* parent, const QString& code, QLabel* status, const QString& name)
{
    const QString compact = licenseCodeCompact(code);
    if (compact.isEmpty()) {
        if (status) {
            status->setText(QString::fromUtf8("还没有%1。").arg(name));
        }
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        parent, QString::fromUtf8("导出%1").arg(name), name + QStringLiteral(".txt"),
        QString::fromUtf8("文本 (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (status) {
            status->setText(QString::fromUtf8("无法写入文件。"));
        }
        return;
    }
    file.write(compact.toUtf8());
    if (status) {
        status->setText(QString::fromUtf8("已导出%1。").arg(name));
    }
}

inline void importLicenseCode(QWidget* parent, QPlainTextEdit* box, QLabel* status, const QString& name)
{
    if (!box) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        parent, QString::fromUtf8("导入%1").arg(name), QString(), QString::fromUtf8("文本 (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (status) {
            status->setText(QString::fromUtf8("无法读取文件。"));
        }
        return;
    }
    const QString compact = licenseCodeCompact(QString::fromUtf8(file.readAll()));
    if (compact.isEmpty()) {
        if (status) {
            status->setText(QString::fromUtf8("文件里没有%1。").arg(name));
        }
        return;
    }
    box->setPlainText(compact);
    if (status) {
        status->setText(QString::fromUtf8("已导入%1。").arg(name));
    }
}

inline void paintLicenseQr(QWidget* parent, const qrcodegen::QrCode& qr, const QString& title);

inline void showLicenseQr(QWidget* parent, const QString& code, const QString& title, QLabel* status)
{
    const QString compact = licenseCodeCompact(code);
    if (compact.isEmpty()) {
        if (status) {
            status->setText(QString::fromUtf8("还没有%1。").arg(title));
        }
        return;
    }
    const QByteArray bytes = compact.toUtf8();
    try {
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
            bytes.constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        paintLicenseQr(parent, qr, title);
    } catch (const std::exception&) {
        if (status) {
            status->setText(QString::fromUtf8("这串码太长，画不出二维码。"));
        }
    }
}

inline void paintLicenseQr(QWidget* parent, const qrcodegen::QrCode& qr, const QString& title)
{
    const int quiet = 4;
    const int modules = qr.getSize();
    int scale = 8;
    while (scale > 3 && (modules + quiet * 2) * scale > 520) {
        --scale;
    }
    const int side = (modules + quiet * 2) * scale;
    QImage image(side, side, QImage::Format_RGB32);
    image.fill(qRgb(255, 255, 255));
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if (!qr.getModule(x, y)) {
                continue;
            }
            for (int py = 0; py < scale; ++py) {
                QRgb* line = reinterpret_cast<QRgb*>(image.scanLine((y + quiet) * scale + py));
                for (int px = 0; px < scale; ++px) {
                    line[(x + quiet) * scale + px] = qRgb(0, 0, 0);
                }
            }
        }
    }
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setObjectName(QStringLiteral("licenseWindow"));
    QLabel* view = new QLabel(&dlg);
    view->setPixmap(QPixmap::fromImage(image));
    view->setAlignment(Qt::AlignCenter);
    QVBoxLayout* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->addWidget(view);
    applyLicenseWindowIcon(&dlg);
    dlg.exec();
}

#endif
