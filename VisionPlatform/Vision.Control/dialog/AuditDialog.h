/**
 * @file AuditDialog.h
 * @brief 审计日志：日期与条件筛选、统计卡片、分页表格、导出 CSV
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_AUDIT_DIALOG_H
#define VISION_CONTROL_AUDIT_DIALOG_H

#include "AuditLog.h"
#include <QDialog>
#include <vector>

class QComboBox;
class QDateEdit;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace Vision {

/**
 * @class AuditDialog
 * @brief 审计日志查看器：按日期读盘，再按设备、事件、操作者和关键字在内存里筛选、统计、分页、导出
 */
class AuditDialog : public QDialog {
public:
    /** @brief 搭界面并按默认区间（本月 1 日到今天）查询；audit 不拥有 */
    AuditDialog(const AuditLog* audit, QWidget* parent = 0);

private:
    /************************************************** 搭建 **************************************************/
    /** @brief 筛选条：日期区间、设备、事件、操作者、关键字、查询、导出 */
    QWidget* buildFilter();
    /** @brief 统计卡片：会话、累计时长、文件传输（未实现）、操作被拒、截图 */
    QWidget* buildStats();
    /** @brief 分页条：总数、页码按钮、每页条数 */
    QWidget* buildPager();
    /** @brief 加一张统计卡片，返回数值标签 */
    QLabel* addCard(QHBoxLayout* row, const QString& title);

    /************************************************** 查询 **************************************************/
    /** @brief 按日期区间读盘（起止颠倒时自动交换），重建下拉选项并筛选 */
    void query();
    /** @brief 在已读记录里按条件筛选，更新统计卡片并回到第一页 */
    void applyFilter();
    /** @brief 显示第 page 页（越界收敛），重建页码按钮（首尾页与当前页前后各一页） */
    void showPage(int page);
    /** @brief 筛选结果导出为带 BOM 的 UTF-8 CSV */
    void exportCsv();
    /**
     * @brief 重填下拉框并尽量保留原选择
     * @param all 首项「全部」的文字，其 data 为空
     * @param values 其余选项，文字与 data 相同
     */
    void refillCombo(QComboBox* combo, const QString& all, const QStringList& values);
    /** @brief 加双引号并转义内部引号 */
    static QString csvField(const QString& text);

    const AuditLog* m_audit;                ///< 审计日志，不拥有
    std::vector<AuditLog::Entry> m_all;     ///< 日期区间内的全部记录，新的在前
    std::vector<AuditLog::Entry> m_rows;    ///< 筛选后的记录，表格与导出用
    int m_page;                             ///< 当前页，从 0 起

    QDateEdit* m_from;          ///< 起始日期
    QDateEdit* m_to;            ///< 结束日期
    QComboBox* m_device;        ///< 设备筛选，data 空为全部
    QComboBox* m_event;         ///< 事件筛选，data 为事件类型，空为全部
    QComboBox* m_op;            ///< 操作者筛选，data 空为全部
    QLineEdit* m_search;        ///< 关键字，匹配设备、来源、详情，回车生效
    QLabel* m_sessions;         ///< 卡片：会话开始次数
    QLabel* m_hours;            ///< 卡片：会话结束记录的累计时长
    QLabel* m_files;            ///< 卡片：文件传输（固定为 -）
    QLabel* m_rejects;          ///< 卡片：操作被拒次数
    QLabel* m_shots;            ///< 卡片：截图次数
    QTableWidget* m_table;      ///< 记录表：时间、设备、操作者、事件、来源、详情
    QLabel* m_total;            ///< 总条数
    QHBoxLayout* m_pages;       ///< 页码按钮所在布局，每次 showPage 重建
    QComboBox* m_perPage;       ///< 每页条数，data 为条数
};

}

#endif
