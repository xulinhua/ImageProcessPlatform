/**
 * @file StationDlg.h
 * @brief StationDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATIONDIALOG_H
#define VISION_STATIONDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>
#include <QString>
#include "RecipeDraft.h"

class QLineEdit;
class QCheckBox;
class QComboBox;
class QTableWidget;
class QSpinBox;
class QListWidget;
class QCloseEvent;

namespace Vision {

class Session;
struct Solution;

/**
 * @class StationDlg
 * @brief 单工站配置：名称、编号、编排、超时、绑定设备、JobCfg 列表
 */
class VISION_UI_API StationDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造工站配置对话框
     * @param stationId 要编辑的工站
     */
    StationDlg(Session* session, Solution* solution, const QString& stationId, QWidget* parent = 0);

signals:
    /**
     * @brief 请求打开选中 JobCfg 的流程配置
     * @param jobId 流程 id
     */
    void openJobRequested(const QString& jobId);

private slots:
    /** @brief 把 draft 提交到 formal 并关闭 */
    void onSave();
    /** @brief 表单变化 */
    void onEdited();
    /** @brief 新建默认 JobCfg（相机+检测） */
    void onAddJob();
    /** @brief 删除当前选中 JobCfg */
    void onDelJob();
    /** @brief 先写回工站属性，再发出 openJobRequested */
    void onOpenJob();
    /** @brief 切换当前流程使能 */
    void onToggleJobEnabled();

protected:
    /** @brief 取消关闭前确认 */
    void reject();
    /** @brief 关闭前确认 */
    void closeEvent(QCloseEvent* event);

private:
    /** @brief 从 draft 填充表单、设备勾选与 JobCfg 表 */
    void load();
    /** @brief 把表单写回 draft */
    void flushUiToDraft();
    /** @brief 更新脏标记 */
    void markDraftDirty();
    /** @brief 关窗前三选一：取消=0，不保存=1，已保存=2 */
    int askCloseAction();
    /** @brief 写回 formal，不关窗 */
    bool trySave();
    /**
     * @brief 当前选中的 JobCfg 行
     * @return 无选中为 -1
     */
    int currentJobRow() const;

    Session* m_session;        ///< 会话（不拥有）
    Solution* m_sol;          ///< 当前方案（不拥有）
    QString m_stationId;      ///< 当前工站
    StationRecipe m_draft;    ///< 工站 draft
    StationRecipe m_formalSnapshot; ///< Open 时 formal 快照
    QLineEdit* m_name;       ///< 工站名称
    QCheckBox* m_enabled;    ///< 工站是否使能
    QLineEdit* m_code;       ///< 工位编号
    QComboBox* m_schedule;   ///< 串行/并行执行
    QSpinBox* m_timeout;      ///< 默认超时 ms
    QTableWidget* m_jobs;    ///< 本工站 JobCfg 表
    QListWidget* m_devices;   ///< 绑定设备勾选
};

}

#endif
