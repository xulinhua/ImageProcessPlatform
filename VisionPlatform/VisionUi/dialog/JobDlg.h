/**
 * @file JobDlg.h
 * @brief JobDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOBDIALOG_H
#define VISION_JOBDIALOG_H
#include "VisionUiExport.h"

#include "Solution.h"
#include "RecipeDraft.h"
#include <QDialog>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QPushButton;
class QPoint;
class QWidget;
class QCloseEvent;

namespace Vision {

class Session;
class JobFlowCanvasPanel;
class Job;
class ToolDebugHost;
class JobRstRuleDlg;
class JobToolTreeDelegate;
struct Solution;

/**
 * @class JobDlg
 * @brief 流程实例编辑：工具库、工具树、画布，以及并行组 / 条件组
 */
class VISION_UI_API JobDlg : public QDialog {
    Q_OBJECT
    friend class JobToolTreeDelegate;
public:
    /**
     * @brief 构造 JobCfg 编辑对话框
     * @param job 流程运行实例（不拥有；可为空则试跑不可用）
     * @param jobId 要编辑的流程实例
     */
    JobDlg(Session* session, Solution* solution, Job* job, const QString& jobId,
              QWidget* parent = 0);

signals:
    /**
     * @brief 请求执行本 JobCfg（保存表头后发出）
     * @param jobId 当前 JobCfg
     */
    void runJobRequested(const QString& jobId);

private slots:
    /** @brief 从工具库把选中工具（或并行/条件组）插入当前节点 */
    void onAdd();
    /** @brief 删除选中工具或组 */
    void onDel();
    /** @brief 在兄弟节点中上移 */
    void onMoveUp();
    /** @brief 在兄弟节点中下移 */
    void onMoveDown();
    /** @brief 重命名选中工具或组 */
    void onRename();
    /** @brief 复制选中工具或组 */
    void onCopy();
    /** @brief 切换选中工具使能 */
    void onToggleToolEnabled();
    /** @brief 在当前节点插入并行组 */
    void onParallel();
    /** @brief 在当前节点插入条件组 */
    void onIf();
    /** @brief 在当前节点插入循环组 */
    void onLoop();
    /** @brief 循环次数变更写回 */
    void onLoopCountChanged(int value);
    /** @brief 给选中的并行组添加一条串行支路 */
    void onAddBranch();
    /** @brief 把条件组的源节点 / Then 条件写回方案模型 */
    void onCondChanged();
    /** @brief 保存 JobCfg draft 并关闭 */
    void onSave();
    /** @brief 页内试跑本流程（用 draft） */
    void onRun();
    /** @brief 页内异步试跑完成 */
    void onAsyncJobRunDone(void* outPtr);
    /**
     * @brief 工具树右键菜单
     * @param pos 相对 m_tools 的点击位置
     */
    void onToolMenu(const QPoint& pos);
    /**
     * @brief 画布选中工具，同步树选中
     * @param toolId 工具 id
     */
    void onCanvasTool(const QString& toolId);
    /**
     * @brief 画布双击工具，打开详细调试
     * @param toolId 工具 id
     */
    void onCanvasToolDouble(const QString& toolId);
    /**
     * @brief 工具树双击：工具进详细调试，组/支路仍重命名
     */
    void onTreeDouble(QTreeWidgetItem* item, int column);
    /** @brief 详细调试窗请求运行本工具 */
    void onDebugRun();
    /** @brief 详细调试窗 ROI 变更 */
    void onDebugRoiChanged();
    /** @brief 详细调试窗 draft 已保存到 formal */
    void onDebugSaved();
    /** @brief 表头字段变化 */
    void onHeaderEdited();
    /** @brief 流程结果选择变更 */
    void onFromRuleChanged();
    /** @brief 输出结果规则变更（配置页确定后） */
    void onRstRulesChanged();
    /** @brief 打开输出结果配置页 */
    void onOpenRstRules();
    /**
     * @brief 画布选中组，同步树选中
     * @param groupId 组 id
     */
    void onCanvasGroup(const QString& groupId);
    /**
     * @brief 画布右键：与工具树相同的结构操作，连线可断开
     */
    void onCanvasMenu(const QString& id, int kind, const QPoint& globalPos);
    /** @brief 按选中节点刷新上移/下移/复制/删除/支路按钮与条件面板 */
    void updateToolButtons();

protected:
    /** @brief 取消关闭前确认是否保存 */
    void reject();
    /** @brief 关闭前确认是否保存 */
    void closeEvent(QCloseEvent* event);

private:
    /** @brief 工具树或画布共用的节点菜单 */
    void popupFlowMenu(const QPoint& globalPos);
    /**
     * @brief 按 JobCfg 树重建工具列表并刷新画布
     * @param selectId 重建后要选中的节点；空则尽量保持当前选中
     */
    void reloadTools(const QString& selectId = QString());
    /** @brief 把名称、超时、失败策略、流程结果写回 draft JobCfg */
    void flushHeaderToDraft();
    /** @brief 刷新「流程结果选择」下拉 */
    void refreshFromRuleCombo();
    /** @brief 更新 draft 脏标记与标题 */
    void markDraftDirty();
    /** @brief 写回 draft 并提交到 formal；失败时弹窗并返回 false */
    bool trySave();
    /**
     * @brief 关闭前询问保存
     * @return 0 取消；1 不保存直接关；2 已保存可关
     */
    int askCloseAction();
    /** @brief 刷新窗口标题 */
    void updateWindowTitle();
    /**
     * @brief 在工具树中选中指定 id
     * @param id 工具或组 id
     */
    void selectInTree(const QString& id);
    /**
     * @brief 把 ToolGroup 递归填入树
     * @param parent 父项；0 表示顶层
     */
    void fillTree(QTreeWidgetItem* parent, const ToolGroup& g, const JobCfg* job);
    /** @brief 按 Job 运行缓存刷新树上绿/红状态点 */
    void refreshToolRunStatus();
    /**
     * @brief 写回节点使能（工具叶子或组内全部叶子）
     * @param item 树节点
     * @param enabled 目标使能
     */
    void applyTreeNodeEnabled(QTreeWidgetItem* item, bool enabled);
    /** @brief 选中条件组时填充条件源与 Then 条件 */
    void fillCondUi();
    /** @brief 选中循环组时填充次数 */
    void fillLoopUi();
    /**
     * @brief 当前树节点 id
     * @return 无选中时为空
     */
    QString currentNodeId() const;
    /**
     * @brief 当前树节点种类（工具 / 并行组 / 条件 / 支路）
     * @return 无选中为 -1
     */
    int currentKind() const;
    /**
     * @brief 用于上移/下移的组节点 id（选中工具时取其叶子组）
     */
    ObjectId moveId() const;
    /** @brief 当前编辑的 JobCfg draft */
    JobCfg* currentJob();
    /** @brief 当前编辑的 JobCfg draft（const） */
    const JobCfg* currentJob() const;
    /** @brief 是否有 PermEditSolution */
    bool canEdit() const;
    /**
     * @brief 打开指定工具的详细调试窗
     * @param toolId 工具 id
     */
    void openToolDebug(const QString& toolId);

    Session* m_session;      ///< 会话（不拥有）
    Solution* m_sol;         ///< 当前方案（不拥有）
    Job* m_job;              ///< 流程运行实例（不拥有）
    QString m_jobId;         ///< 当前 JobCfg
    JobCfg m_draftJob;          ///< 本会话 JobCfg draft
    JobCfg m_formalSnapshot;    ///< Open 时 formal 快照
    QLineEdit* m_name;       ///< JobCfg 名称
    QCheckBox* m_enabled;    ///< 流程是否使能
    QSpinBox* m_timeout;     ///< 超时 ms
    QComboBox* m_fail;       ///< 失败策略
    QComboBox* m_fromRule;   ///< 流程结果选择
    QTreeWidget* m_lib;      ///< 左侧工具库
    QTreeWidget* m_tools;    ///< JobCfg 工具树
    JobFlowCanvasPanel* m_flowCanvas; ///< 流程画布（含连线写回）
    ToolDebugHost* m_debug;  ///< 详细调试窗（含参数与 ROI）
    QString m_debugToolId;   ///< 当前调试窗对应工具 id
    QPushButton* m_up;       ///< 上移
    QPushButton* m_down;     ///< 下移
    QPushButton* m_rename;   ///< 重命名
    QPushButton* m_copy;     ///< 复制
    QPushButton* m_del;      ///< 删除
    QPushButton* m_branch;   ///< 添加并行支路
    QPushButton* m_ifBtn;    ///< 插入条件组
    QPushButton* m_loopBtn;  ///< 插入循环组
    QPushButton* m_btnRstRules; ///< 打开输出结果配置页
    QWidget* m_condPanel;    ///< 条件组属性面板
    QComboBox* m_condSrc;    ///< 条件源节点
    QComboBox* m_condWant;   ///< Then 当 OK 或 NG
    QWidget* m_loopPanel;    ///< 循环组属性面板
    QSpinBox* m_loopCount;   ///< 循环次数
};

}

#endif
