/**
 * @file LogDlg.h
 * @brief LogDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LOGDIALOG_H
#define VISION_LOGDIALOG_H
#include "VisionUiExport.h"

#include "LogMgr.h"
#include <QDialog>

class QTabWidget;
class QTableWidget;
class QDateEdit;
class QListWidget;
class QPushButton;
class QLabel;

namespace Vision {

class Session;

/**
 * @class LogDlg
 * @brief 日志管控：操作日志、调试日志查询，以及按日期删除数据库记录
 */
class VISION_UI_API LogDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造
     * @param session 会话（不拥有），用于权限与写删除审计
     */
    explicit LogDlg(Session* session, QWidget* parent = 0);

private slots:
    /** @brief 刷新当前页表格 */
    void onRefresh();
    /** @brief 切 Tab 后刷新 */
    void onTabChanged(int index);
    /** @brief 删除勾选日期的日志 */
    void onDelete();

private:
    /** @brief 把查询结果填进表 */
    void fillTable(QTableWidget* table, LogKind kind);
    /** @brief 刷新文件列表 */
    void fillFiles();
    /** @brief 当前用户是否可删日志 */
    bool canManage() const;

    Session* m_session;          ///< 会话
    QTabWidget* m_tabs;          ///< 三个页
    QTableWidget* m_opTable;     ///< 操作日志
    QTableWidget* m_dbgTable;    ///< 调试日志
    QDateEdit* m_from;           ///< 起始日
    QDateEdit* m_to;             ///< 结束日
    QListWidget* m_files;        ///< 可删日期
    QPushButton* m_btnDelete;    ///< 删除所选
    QLabel* m_fileHint;          ///< 管理页说明
};

}

#endif
