/**
 * @file debug_theme.h
 * @brief 进程通讯调试器自己的配色和控件样式
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 只给 HsIpcDebug 用。设备通讯调试器有自己的一份，两边以后可以改成不同主题。
 */

#ifndef HS_IPC_DEBUG_THEME_H
#define HS_IPC_DEBUG_THEME_H

#include <QApplication>
#include <QString>

/** @brief 本调试器外观。不依赖视觉平台的主题库 */
class DebugTheme {
public:
    /** @brief 主色，打开按钮和选中页签 */
    static const char* primary() { return "#1677FF"; }
    /** @brief 已连接 */
    static const char* statusOn() { return "#237804"; }
    /** @brief 未连接 */
    static const char* statusOff() { return "#8C8C8C"; }

    /** @brief 装到本进程 */
    static void apply(QApplication* app)
    {
        if (!app) {
            return;
        }
        app->setStyleSheet(QString::fromUtf8(
            "QWidget { font-family: 'Microsoft YaHei UI'; font-size: 12px; color: #1F1F1F; }"
            "QMainWindow { background: #F5F6F8; }"
            "QFrame#panel { background: #FFFFFF; border: 1px solid #E5E6EB; border-radius: 6px; }"
            "QLabel#hint { color: #8C8C8C; }"
            "QLabel#statusOn { color: #237804; font-weight: bold; }"
            "QLabel#statusOff { color: #8C8C8C; }"
            "QPushButton { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px;"
            "  padding: 4px 14px; min-height: 22px; }"
            "QPushButton:hover { border-color: #1677FF; color: #1677FF; }"
            "QPushButton:disabled { color: #BFBFBF; background: #F5F5F5; border-color: #F0F0F0; }"
            "QPushButton#primary { background: #1677FF; color: #FFFFFF; border: none; }"
            "QPushButton#primary:hover { background: #4096FF; color: #FFFFFF; }"
            "QPushButton#primary:disabled { background: #BAE0FF; color: #FFFFFF; }"
            "QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {"
            "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 2px 6px; }"
            "QPlainTextEdit#log { font-family: Consolas, 'Courier New'; font-size: 12px; }"
            "QTableView { background: #FFFFFF; border: 1px solid #E5E6EB; gridline-color: #F0F0F0;"
            "  selection-background-color: #E6F4FF; selection-color: #1F1F1F; }"
            "QHeaderView::section { background: #FAFAFA; border: none; border-bottom: 1px solid #E5E6EB;"
            "  border-right: 1px solid #F0F0F0; padding: 4px 6px; }"
            "QTabBar::tab { background: #F5F6F8; border: 1px solid #E5E6EB; padding: 6px 14px; margin-right: 2px; }"
            "QTabBar::tab:selected { background: #FFFFFF; color: #1677FF; border-bottom: 2px solid #1677FF; }"
            "QListWidget { background: #FFFFFF; border: 1px solid #E5E6EB; border-radius: 4px; }"
            "QStatusBar { background: #F5F6F8; color: #595959; }"));
    }
};

#endif
