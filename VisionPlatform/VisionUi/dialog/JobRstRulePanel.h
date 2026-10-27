/**
 * @file JobRstRulePanel.h
 * @brief JobRstRulePanel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOBRESULTRULEPANEL_H
#define VISION_JOBRESULTRULEPANEL_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QWidget>

class QComboBox;
class QTableWidget;
class QPushButton;
class QCheckBox;

namespace Vision {

/**
 * @class JobRstRulePanel
 * @brief JobCfg 输出结果配置：逻辑判断矩阵与「流程结果取自」
 */
class VISION_UI_API JobRstRulePanel : public QWidget {
    Q_OBJECT
public:
    /** @brief 输出结果配置面板 */
    explicit JobRstRulePanel(QWidget* parent = 0);

    /** @brief 绑定当前编辑中的 JobCfg（不拥有） */
    void setJob(JobCfg* job);
    /** @brief 从 JobCfg 刷新表格 */
    void loadFromJob(const JobCfg* job);
    /** @brief 写回 JobCfg */
    void flushToJob(JobCfg* job);
    /** @brief 是否允许编辑 */
    void setEditable(bool editable);
    /** @brief 填充「流程结果选择」下拉 */
    static void populateFromRuleCombo(QComboBox* combo, const JobCfg* job);
    /** @brief 从下拉写回 resultFromRuleId */
    static void applyFromRuleCombo(QComboBox* combo, JobCfg* job);

signals:
    /** @brief 规则或取自项变更 */
    void changed();

private slots:
    /** @brief 三态格单击循环 */
    void onTermClicked();
    /** @brief 绑定工具或名称变更 */
    void onRuleMetaChanged();
    /** @brief 添加一行逻辑判断 */
    void onAddRule();
    /** @brief 删除选中行 */
    void onRemoveRule();

private:
    /** @brief 重建表头与行 */
    void rebuildTable();
    /** @brief 设置三态 checkbox 外观 */
    static void applyTermVisual(QCheckBox* cb, int state);
    /** @brief 当前列对应的来源 kind 与 id */
    bool columnSource(int col, int ruleRow, int* kind, ObjectId* sourceId) const;

    JobCfg* m_editJob;           ///< 当前编辑 JobCfg（不拥有）
    bool m_editable;          ///< 是否可编辑
    QTableWidget* m_table;  ///< 逻辑矩阵
    QPushButton* m_addRule;   ///< 添加行
    QPushButton* m_removeRule; ///< 删除行
};

}

#endif
