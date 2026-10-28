/**
 * @file LicenseUiTheme.h
 * @brief 注册窗口和注册机共用浅色工业配色，和主程序同一套色
 */

#ifndef VISION_LICENSE_UI_THEME_H
#define VISION_LICENSE_UI_THEME_H

#include <QApplication>
#include <QString>
#include <QStyle>
#include <QWidget>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

inline void applyLicenseUiTheme(QApplication* app)
{
    if (!app) {
        return;
    }
    app->setStyleSheet(QString::fromUtf8(
        "QWidget { color: #333333; font-family: 'Microsoft YaHei','Segoe UI'; font-size: 13px; }"
        "QWidget#licenseWindow { background: #F5F7FA; }"
        "QLabel { background: transparent; color: #333333; }"
        "QLabel#product { font-size: 15px; font-weight: bold; }"
        "QLabel#section { color: #1890FF; font-weight: bold; }"
        "QLabel#hint { color: #888888; }"
        "QLabel#keyName { color: #888888; }"
        "QLabel#pillOk { color: #237804; background: #F6FFED; border: 1px solid #B7EB8F; border-radius: 10px; padding: 2px 8px; }"
        "QLabel#pillBad { color: #A8071A; background: #FFF1F0; border: 1px solid #FFA39E; border-radius: 10px; padding: 2px 8px; }"
        "QLabel#pillIdle { color: #595959; background: #FAFAFA; border: 1px solid #D9D9D9; border-radius: 10px; padding: 2px 8px; }"
        "QPlainTextEdit { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 6px; }"
        "QPlainTextEdit#codeBox { font-family: Consolas,'Courier New'; font-size: 12px; }"
        "QPlainTextEdit:focus { border: 1px solid #1890FF; }"
        "QLineEdit, QSpinBox {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 4px 8px; min-height: 22px;"
        "}"
        "QLineEdit:focus, QSpinBox:focus { border: 1px solid #1890FF; }"
        "QLineEdit:disabled, QSpinBox:disabled { color: #BBBBBB; background: #F5F5F5; }"
        "QPushButton { background: #FFFFFF; color: #333333; border: 1px solid #D9D9D9; border-radius: 4px; padding: 6px 16px; min-height: 26px; }"
        "QPushButton:hover { border-color: #1890FF; color: #1890FF; }"
        "QPushButton:pressed { background: #E6F7FF; }"
        "QPushButton:disabled { color: #BBBBBB; background: #F5F5F5; border-color: #D9D9D9; }"
        "QPushButton[primary=\"true\"] { background: #1890FF; color: #FFFFFF; border: none; }"
        "QPushButton[primary=\"true\"]:hover { background: #40A9FF; color: #FFFFFF; }"
        "QPushButton[primary=\"true\"]:disabled { background: #F5F5F5; color: #BBBBBB; }"
        "QRadioButton { spacing: 6px; }"
        "QMessageBox { background: #F5F7FA; }"
        "QCalendarWidget QWidget { background: #FFFFFF; color: #333333; }"
        "QCalendarWidget QToolButton { color: #333333; background: #FFFFFF; }"
        "QCalendarWidget QAbstractItemView:enabled { color: #333333; selection-background-color: #1890FF; selection-color: #FFFFFF; }"
    ));
}

inline void applyLicenseWindowIcon(QWidget* widget)
{
    if (!widget) {
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) {
        return;
    }
    HICON big = reinterpret_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    HICON small = reinterpret_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (big) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    }
    if (small) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
    }
}

inline void polishWidget(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

#endif
