/**
 * @file RemoteTheme.cpp
 * @brief 样式表与图标
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RemoteTheme.h"
#include "RemoteStyle.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QSvgRenderer>
#include <QVariant>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstring>

#pragma comment(lib, "gdiplus.lib")

namespace Vision {

const char* const RemoteTheme::kPrimary = "#1890FF";
const char* const RemoteTheme::kSuccess = "#52C41A";
const char* const RemoteTheme::kWarning = "#FAAD14";
const char* const RemoteTheme::kDanger = "#FF4D4F";
const char* const RemoteTheme::kText = "#262626";
const char* const RemoteTheme::kMuted = "#8C8C8C";
const char* const RemoteTheme::kBorder = "#E8E8E8";
const char* const RemoteTheme::kPage = "#F5F7FA";

const char* RemoteTheme::body(const char* name)
{
    struct Entry {
        const char* name;
        const char* body;
    };
    // 24×24 视口的线条图标，只存 <svg> 内部元素；fill="currentColor" 的部分在 svg() 里换色
    static const Entry kIcons[] = {
        { "plus", R"(<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>)" },
        { "minus", R"(<line x1="5" y1="12" x2="19" y2="12"/>)" },
        { "x", R"(<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>)" },
        { "refresh", R"(<polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>)" },
        { "search", R"(<circle cx="11" cy="11" r="7"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>)" },
        { "play", R"(<polygon points="6 4 19 12 6 20 6 4"/>)" },
        { "left", R"(<polyline points="15 18 9 12 15 6"/>)" },
        { "right", R"(<polyline points="9 18 15 12 9 6"/>)" },
        { "down", R"(<polyline points="6 9 12 15 18 9"/>)" },
        { "monitor", R"(<rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/>)" },
        { "app", R"(<rect x="3" y="4" width="18" height="16" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/>)" },
        { "mouse", R"(<path d="M3 3l7.07 16.97 2.51-7.39 7.39-2.51L3 3z"/><path d="M13 13l6 6"/>)" },
        { "clipboard", R"(<path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2"/><rect x="8" y="2" width="8" height="4" rx="1"/>)" },
        { "folder", R"(<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>)" },
        { "file", R"(<path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/>)" },
        { "chat", R"(<path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>)" },
        { "camera", R"(<path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/><circle cx="12" cy="13" r="4"/>)" },
        { "record", R"(<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4" fill="currentColor"/>)" },
        { "power", R"(<path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/>)" },
        { "lock", R"(<rect x="4" y="11" width="16" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>)" },
        { "full", R"(<path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"/>)" },
        { "fit", R"(<polyline points="4 14 10 14 10 20"/><polyline points="20 10 14 10 14 4"/><line x1="14" y1="10" x2="21" y2="3"/><line x1="3" y1="21" x2="10" y2="14"/>)" },
        { "up", R"(<line x1="12" y1="19" x2="12" y2="5"/><polyline points="5 12 12 5 19 12"/>)" },
        { "upload", R"(<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>)" },
        { "download", R"(<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>)" },
        { "toRight", R"(<line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/>)" },
        { "toLeft", R"(<line x1="19" y1="12" x2="5" y2="12"/><polyline points="12 19 5 12 12 5"/>)" },
        { "sliders", R"(<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/>)" },
        { "shield", R"(<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>)" },
        { "shieldOk", R"(<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><polyline points="8.5 12 11 14.5 15.5 9.5"/>)" },
        { "user", R"(<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>)" },
        { "copy", R"(<rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>)" },
        { "keyboard", R"(<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/>)" },
        { "pause", R"(<rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/>)" },
        { "check", R"(<polyline points="20 6 9 17 4 12"/>)" },
        { "alert", R"(<path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>)" },
        { "eye", R"(<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>)" },
        { "link", R"(<path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/>)" },
        { "send", R"(<line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/>)" },
        { "zap", R"(<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>)" },
        { "server", R"(<rect x="2" y="2" width="20" height="8" rx="2"/><rect x="2" y="14" width="20" height="8" rx="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/>)" },
        { "wifi", R"(<path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/>)" },
        { "list", R"(<line x1="8" y1="6" x2="21" y2="6"/><line x1="8" y1="12" x2="21" y2="12"/><line x1="8" y1="18" x2="21" y2="18"/><line x1="3" y1="6" x2="3.01" y2="6"/><line x1="3" y1="12" x2="3.01" y2="12"/><line x1="3" y1="18" x2="3.01" y2="18"/>)" },
        { "calendar", R"(<rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/>)" },
        { "film", R"(<rect x="2" y="2" width="20" height="20" rx="2.18"/><line x1="7" y1="2" x2="7" y2="22"/><line x1="17" y1="2" x2="17" y2="22"/><line x1="2" y1="12" x2="22" y2="12"/><line x1="2" y1="7" x2="7" y2="7"/><line x1="2" y1="17" x2="7" y2="17"/><line x1="17" y1="17" x2="22" y2="17"/><line x1="17" y1="7" x2="22" y2="7"/>)" },
        { "info", R"(<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>)" },
        { "win", R"(<rect x="3" y="3" width="8" height="8" fill="currentColor" stroke="none"/><rect x="13" y="3" width="8" height="8" fill="currentColor" stroke="none"/><rect x="3" y="13" width="8" height="8" fill="currentColor" stroke="none"/><rect x="13" y="13" width="8" height="8" fill="currentColor" stroke="none"/>)" },
        { "tab", R"(<polyline points="15 6 21 12 15 18"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="21" y1="5" x2="21" y2="19"/>)" },
        { "globe", R"(<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>)" },
        { "hdd", R"(<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/>)" },
        { "image", R"(<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>)" },
        { "clock", R"(<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>)" },
        { "edit", R"(<path d="M12 20h9"/><path d="M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"/>)" },
        { "trash", R"(<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>)" },
        { "grid", R"(<rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>)" }
    };
    // 图标数量少，线性查找即可
    for (size_t i = 0; i < sizeof(kIcons) / sizeof(kIcons[0]); ++i) {
        if (std::strcmp(kIcons[i].name, name) == 0) {
            return kIcons[i].body;
        }
    }
    return "";
}

QByteArray RemoteTheme::svg(const char* name, const QColor& color)
{
    QByteArray inner(body(name));
    const QByteArray hex = color.name().toLatin1();
    inner.replace("currentColor", hex);
    return QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"")
        + hex + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">" + inner + "</svg>";
}

QPixmap RemoteTheme::pixmap(const char* name, const QColor& color, int px, qreal dpr)
{
    const int side = static_cast<int>(px * dpr + 0.5);
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg(name, color));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, side, side));
    painter.end();
    QPixmap out = QPixmap::fromImage(image);
    out.setDevicePixelRatio(dpr);
    return out;
}

QIcon RemoteTheme::icon(const char* name, const QColor& color, int px)
{
    QIcon out;
    const QColor gray("#BFBFBF");
    out.addPixmap(pixmap(name, color, px, 1.0), QIcon::Normal);
    out.addPixmap(pixmap(name, color, px, 2.0), QIcon::Normal);
    out.addPixmap(pixmap(name, gray, px, 1.0), QIcon::Disabled);
    out.addPixmap(pixmap(name, gray, px, 2.0), QIcon::Disabled);
    return out;
}

void RemoteTheme::setProp(QWidget* widget, const char* key, const QVariant& value)
{
    if (!widget) {
        return;
    }
    widget->setProperty(key, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QPushButton* RemoteTheme::button(const QString& text, const char* iconName, const char* kind, QWidget* parent)
{
    QPushButton* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    const QByteArray k(kind ? kind : "");
    if (!k.isEmpty()) {
        button->setProperty("kind", QString::fromLatin1(k));
    }
    if (iconName && *iconName) {
        QColor color(kText);
        if (k == "primary" || k == "dangerFill") {
            color = QColor("#FFFFFF");
        } else if (k == "danger") {
            color = QColor(kDanger);
        }
        button->setIcon(icon(iconName, color));
        button->setIconSize(QSize(15, 15));
    }
    return button;
}

void RemoteTheme::markLater(QWidget* widget, const QString& phase)
{
    if (!widget) {
        return;
    }
    widget->setEnabled(false);
    widget->setToolTip(QString::fromUtf8("%1提供").arg(phase));
}

QImage RemoteTheme::imageFromBgra(const unsigned char* data, int size, int width, int height)
{
    if (!data || width < 1 || height < 1) {
        return QImage();
    }
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    if (size < 0 || static_cast<size_t>(size) < need) {
        return QImage();
    }
    // RGB32 在小端内存布局与 BGRA 一致，且绘制时按不透明处理（忽略 alpha=0）
    const QImage view(data, width, height, width * 4, QImage::Format_RGB32);
    return view.copy();
}

QImage RemoteTheme::imageFromJpeg(const unsigned char* data, int size)
{
    if (!data || size <= 0) {
        return QImage();
    }
    // Debug 下 QImage::loadFromData(..., "JPEG") 会对部分 GDI+ 输出触发 fail-fast；GDI+ 解码更稳
    static ULONG_PTR token = 0;
    static bool ready = false;
    if (!ready) {
        Gdiplus::GdiplusStartupInput input;
        ready = (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok);
    }
    if (!ready) {
        return QImage();
    }
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(size));
    if (!mem) {
        return QImage();
    }
    void* ptr = GlobalLock(mem);
    if (!ptr) {
        GlobalFree(mem);
        return QImage();
    }
    std::memcpy(ptr, data, static_cast<size_t>(size));
    GlobalUnlock(mem);
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(mem, TRUE, &stream) != S_OK || !stream) {
        GlobalFree(mem);
        return QImage();
    }
    Gdiplus::Bitmap bitmap(stream);
    stream->Release();
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        return QImage();
    }
    const int w = static_cast<int>(bitmap.GetWidth());
    const int h = static_cast<int>(bitmap.GetHeight());
    if (w <= 0 || h <= 0) {
        return QImage();
    }
    QImage image(w, h, QImage::Format_ARGB32);
    if (image.isNull()) {
        return QImage();
    }
    Gdiplus::BitmapData bits;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bits) != Gdiplus::Ok) {
        return QImage();
    }
    for (int y = 0; y < h; ++y) {
        const unsigned char* src = static_cast<const unsigned char*>(bits.Scan0) + y * bits.Stride;
        std::memcpy(image.scanLine(y), src, static_cast<size_t>(w) * 4);
    }
    bitmap.UnlockBits(&bits);
    return image;
}

void RemoteTheme::apply(QApplication* app)
{
    QFont font(QString::fromUtf8("Microsoft YaHei UI"));
    font.setPixelSize(13);
    app->setFont(font);
    app->setStyle(new RemoteStyle(QStyleFactory::create("Fusion")));

    QPalette pal = app->palette();
    pal.setColor(QPalette::Window, QColor(kPage));
    pal.setColor(QPalette::Base, QColor("#FFFFFF"));
    pal.setColor(QPalette::AlternateBase, QColor("#FAFAFA"));
    pal.setColor(QPalette::Text, QColor(kText));
    pal.setColor(QPalette::WindowText, QColor(kText));
    pal.setColor(QPalette::ButtonText, QColor(kText));
    pal.setColor(QPalette::Button, QColor("#FFFFFF"));
    pal.setColor(QPalette::Highlight, QColor("#E6F4FF"));
    pal.setColor(QPalette::HighlightedText, QColor(kText));
    pal.setColor(QPalette::PlaceholderText, QColor("#BFBFBF"));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#BFBFBF"));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#BFBFBF"));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#BFBFBF"));
    app->setPalette(pal);

    const QString dir = QDir::tempPath() + "/VisionRemoteUi";
    QDir().mkpath(dir);
    const QString arrow = dir + "/down.png";
    pixmap("down", QColor(kMuted), 12, 2.0).toImage().save(arrow, "PNG");
    app->setStyleSheet(styleSheet(arrow));
}

QString RemoteTheme::styleSheet(const QString& arrow)
{
    // 依次为：菜单、容器（toolbar/statusbar/side/panel/card）、按钮（kind/size/seg 属性）、输入框、标签页、
    // 表格与列表、滚动条、导航、进度条与滑块、文字标签（role/tone 属性）
    QString qss = QString::fromUtf8(R"(
QWidget { color:#262626; }
QLabel, QCheckBox, QRadioButton { background:transparent; }
QToolTip { background:#262626; color:#FFFFFF; border:none; padding:4px 8px; }

QMenuBar { background:#FFFFFF; border-bottom:1px solid #E8E8E8; padding:2px 6px; }
QMenuBar::item { padding:5px 10px; background:transparent; border-radius:4px; }
QMenuBar::item:selected { background:#F0F5FF; color:#1677FF; }
QMenu { background:#FFFFFF; border:1px solid #E8E8E8; padding:4px; }
QMenu::item { padding:6px 28px 6px 26px; border-radius:4px; }
QMenu::item:selected { background:#E6F4FF; color:#1677FF; }
QMenu::item:disabled { color:#BFBFBF; background:transparent; }
QMenu::separator { height:1px; background:#F0F0F0; margin:4px 8px; }
QMenu::icon { padding-left:8px; }

QWidget#toolbar, QWidget#statusbar, QWidget#side, QWidget#panel, QWidget#white { background:#FFFFFF; }
QWidget#toolbar { border-bottom:1px solid #E8E8E8; }
QWidget#statusbar { border-top:1px solid #E8E8E8; }
QWidget#statusbar QLabel { color:#8C8C8C; font-size:12px; }
QWidget#side { border-right:1px solid #E8E8E8; }
QWidget#panel { border-left:1px solid #E8E8E8; }
QWidget#footer { background:#FAFAFA; border-top:1px solid #F0F0F0; }
QFrame#card { background:#FFFFFF; border:1px solid #E8E8E8; border-radius:6px; }
QFrame#sep { background:#E8E8E8; max-width:1px; min-width:1px; }

QPushButton { background:#FFFFFF; border:1px solid #D9D9D9; border-radius:4px; padding:0 12px; min-height:28px; color:#262626; }
QPushButton:hover { border-color:#4096FF; color:#4096FF; }
QPushButton:pressed { border-color:#0958D9; color:#0958D9; }
QPushButton:checked { background:#E6F4FF; border-color:#91CAFF; color:#1677FF; }
QPushButton:disabled { background:#F5F5F5; border-color:#D9D9D9; color:#BFBFBF; }
QPushButton[kind="primary"] { background:#1890FF; border-color:#1890FF; color:#FFFFFF; }
QPushButton[kind="primary"]:hover { background:#4096FF; border-color:#4096FF; color:#FFFFFF; }
QPushButton[kind="primary"]:pressed { background:#0958D9; border-color:#0958D9; }
QPushButton[kind="primary"]:disabled { background:#F5F5F5; border-color:#D9D9D9; color:#BFBFBF; }
QPushButton[kind="danger"] { color:#FF4D4F; border-color:#FFA39E; }
QPushButton[kind="danger"]:hover { color:#FF7875; border-color:#FF7875; }
QPushButton[kind="dangerFill"] { background:#FF4D4F; border-color:#FF4D4F; color:#FFFFFF; }
QPushButton[kind="dangerFill"]:hover { background:#FF7875; border-color:#FF7875; color:#FFFFFF; }
QPushButton[kind="icon"] { padding:0; min-width:28px; max-width:28px; }
QPushButton[size="sm"] { min-height:24px; padding:0 10px; font-size:12px; }
QPushButton[kind="icon"][size="sm"] { min-width:24px; max-width:24px; padding:0; }
QPushButton[seg="first"] { border-top-right-radius:0; border-bottom-right-radius:0; }
QPushButton[seg="mid"] { border-radius:0; border-left:none; }
QPushButton[seg="last"] { border-top-left-radius:0; border-bottom-left-radius:0; border-left:none; }
QPushButton[seg="mid"]:checked, QPushButton[seg="last"]:checked { border-left:1px solid #91CAFF; }
QPushButton[kind="link"] { border:none; background:transparent; color:#1677FF; padding:0; min-height:20px; }

QLineEdit, QSpinBox, QComboBox, QDateEdit { background:#FFFFFF; border:1px solid #D9D9D9; border-radius:4px; padding:0 8px; min-height:28px; selection-background-color:#91CAFF; selection-color:#262626; }
QLineEdit:hover, QSpinBox:hover, QComboBox:hover, QDateEdit:hover { border-color:#4096FF; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QDateEdit:focus { border-color:#1890FF; }
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled, QDateEdit:disabled { background:#F5F5F5; color:#BFBFBF; border-color:#D9D9D9; }
QLineEdit[size="sm"], QComboBox[size="sm"] { min-height:24px; font-size:12px; }
QSpinBox::up-button, QSpinBox::down-button { width:0; border:none; }
QDateEdit::up-button, QDateEdit::down-button { width:0; border:none; }
QComboBox::drop-down, QDateEdit::drop-down { border:none; width:24px; subcontrol-origin:padding; subcontrol-position:center right; }
QComboBox::down-arrow, QDateEdit::down-arrow { image:url(ARROW); width:12px; height:12px; }
QComboBox QAbstractItemView { background:#FFFFFF; border:1px solid #E8E8E8; outline:0; padding:4px; selection-background-color:#E6F4FF; selection-color:#1677FF; }

QTabWidget::pane { border:none; border-top:1px solid #F0F0F0; background:#FFFFFF; }
QTabBar { background:transparent; }
QTabBar::tab { background:transparent; border:none; padding:10px 2px 8px 2px; margin-right:22px; color:#595959; }
QTabBar::tab:selected { color:#1677FF; border-bottom:2px solid #1677FF; }
QTabBar::tab:hover { color:#1677FF; }
QTabBar::tab:disabled { color:#BFBFBF; }

QTableView, QTableWidget { background:#FFFFFF; border:none; gridline-color:transparent; selection-background-color:#E6F4FF; selection-color:#262626; font-size:12px; outline:0; }
QTableView::item { border-bottom:1px solid #F5F5F5; padding:0 10px; }
QHeaderView { background:#FAFAFA; border:none; }
QHeaderView::section { background:#FAFAFA; color:#8C8C8C; border:none; border-bottom:1px solid #F0F0F0; padding:0 10px; min-height:32px; font-size:12px; }
QTreeView, QTreeWidget, QListView, QListWidget { background:#FFFFFF; border:none; outline:0; }
QAbstractScrollArea { background:transparent; }

QScrollBar:vertical { background:transparent; width:8px; margin:0; }
QScrollBar::handle:vertical { background:#D9D9D9; border-radius:4px; min-height:30px; }
QScrollBar:horizontal { background:transparent; height:8px; margin:0; }
QScrollBar::handle:horizontal { background:#D9D9D9; border-radius:4px; min-width:30px; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }

QListWidget#nav { background:#FAFBFC; border-right:1px solid #F0F0F0; padding:10px 8px; }
QListWidget#nav::item { height:38px; padding-left:8px; border-radius:4px; color:#595959; }
QListWidget#nav::item:selected { background:#E6F4FF; color:#1677FF; }
QListWidget#nav::item:hover:!selected { background:#F5F5F5; }

QProgressBar { background:#F0F0F0; border:none; border-radius:3px; max-height:6px; min-height:6px; }
QProgressBar::chunk { background:#1890FF; border-radius:3px; }
QProgressBar[tone="green"]::chunk { background:#52C41A; }
QProgressBar[tone="red"]::chunk { background:#FF4D4F; }
QProgressBar[tone="orange"]::chunk { background:#FA8C16; }
QSlider::groove:horizontal { height:4px; background:#F0F0F0; border-radius:2px; }
QSlider::sub-page:horizontal { background:#1890FF; border-radius:2px; }
QSlider::handle:horizontal { background:#FFFFFF; border:2px solid #1890FF; width:12px; height:12px; margin:-6px 0; border-radius:8px; }
QSlider::sub-page:horizontal:disabled { background:#D9D9D9; }
QSlider::handle:horizontal:disabled { border-color:#D9D9D9; }

QLabel[role="muted"] { color:#8C8C8C; }
QLabel[role="small"] { color:#8C8C8C; font-size:12px; }
QLabel[role="section"] { font-weight:600; font-size:14px; }
QLabel[role="key"] { color:#595959; }
QLabel[role="mono"] { font-family:Consolas,"Microsoft YaHei UI"; }
QLabel[tone] { border-radius:3px; padding:1px 6px; font-size:11px; }
QLabel[tone="gray"] { background:#F0F0F0; color:#595959; }
QLabel[tone="blue"] { background:#E6F4FF; color:#1677FF; }
QLabel[tone="green"] { background:#F6FFED; color:#389E0D; }
QLabel[tone="red"] { background:#FFF1F0; color:#CF1322; }
QLabel[tone="orange"] { background:#FFF7E6; color:#D46B08; }
QLabel[tone="purple"] { background:#F9F0FF; color:#531DAB; }
)");
    // 箭头是运行时渲染的图标，apply() 先存成临时 PNG，这里把路径填进 image:url()
    qss.replace("ARROW", arrow);
    return qss;
}

}
