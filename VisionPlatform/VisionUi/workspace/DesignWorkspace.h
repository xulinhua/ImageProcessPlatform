/**
 * @file DesignWorkspace.h
 * @brief DesignWorkspace头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DESIGNWORKSPACE_H
#define VISION_DESIGNWORKSPACE_H
#include "VisionUiExport.h"

#include <HsCtrl/HsImageWindow.h>
#include <HsCtrl/HsViewShell.h>
#include "ToolBase.h"
#include <QVector>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QLabel;
class QPushButton;
class QPoint;

namespace Vision {

class JobFlowCanvasPanel;
class Session;
class Station;
class ToolDebugHost;
class ToolRstPanel;
struct Solution;
struct ToolCfg;
struct JobCfg;

/**
 * @class DesignWorkspace
 * @brief 设计工作区：方案树+算法库 | 流程画布 | 图像与调试
 */
class VISION_UI_API DesignWorkspace : public QWidget {
    Q_OBJECT
public:
    /** @brief session / solution / station 均不拥有 */
    DesignWorkspace(Session* session, Solution* solution, Station* station, QWidget* parent = 0);

    /** @brief 绑定工站运行实例（不拥有；方案切换后由主窗重建） */
    void setStation(Station* station);

    /** @brief 重刷方案树（工站/流程，不含工具） */
    void reloadTree();
    /** @brief 按当前已登记工具重填工具库 */
    void reloadToolLib();
    /** @brief 追加一行日志（整行进消息列） */
    void appendLog(const QString& line);
    /**
     * @brief 追加运行日志（级别与消息分列；消息不含 UUID）
     * @param level 级别文案，如 OK / NG / INF
     * @param message 正文
     */
    void appendLog(const QString& level, const QString& message);
    /**
     * @brief 显示某工具的图与叠加，并刷新右侧结果区
     * @param runMeta 非空且与展示工具 id 一致时写入本次运行摘要
     * @param afterJobRun true 表示刚跑完整流程
     */
    void showToolResult(const QString& toolId, const HsCommData::HsImage& fb,
                        const QVector<HsCommData::Graphic>& overlay,
                        const ToolOut* runMeta = 0, bool afterJobRun = false);
    /**
     * @brief 按缓存刷新当前工具画面
     * @param preferInput true 时优先显示输入图（方便画 ROI）
     */
    void syncToolView(bool preferInput);
    /** @brief 设计图像窗 */
    HsCtrl::HsImageWindow* imageWindow() const;
    /** @brief 当前工具 id */
    QString currentToolId() const;
    /** @brief 当前 JobCfg id */
    QString currentJobId() const;
    /** @brief 当前关联工站 id（树选中或当前流程所属） */
    QString currentStationId() const;
    /** @brief 主页不 flush 调试窗到 formal；保留接口供 MainWindow 调用 */
    void applyParamsFromUi();
    /** @brief 按权限启用调试运行按钮 */
    void setDebugPerms(bool canDebug);
    /** @brief 语言热切换后刷新静态文案 */
    void retranslateUi();
    /** @brief 跑完整流程后，按当前选中工具刷新图像与结果区 */
    void syncAfterJobRun(const ToolOut& jobOut);
    /**
     * @brief 复检跳转：选中流程/工具并打开详细调试（可强制复检图路径）
     * @param jobId 流程 id
     * @param toolId 工具 id（可空=流程首个工具）
     * @param imagePath 复检图全路径（可空）
     */
    void focusRecheckDebug(const QString& jobId, const QString& toolId, const QString& imagePath);

signals:
    /** @brief 双击方案根：请求打开工站管理 */
    void stationsRequested();
    /** @brief 选中工站 */
    void stationActivated(const QString& stationId);
    /** @brief 选中 JobCfg */
    void jobActivated(const QString& jobId);
    /** @brief 选中工具 */
    void toolActivated(const QString& toolId);
    /** @brief 显示名变化，需刷标题 */
    void namesChanged();
    /** @brief 请求跑当前工具 */
    void runThisTool();
    /** @brief 请求跑当前 JobCfg */
    void runThisJob();

public slots:
    /** @brief HUD 光标信息 */
    void onCursor(const QPointF& pt, double gray, bool valid);

private slots:
    /** @brief 树双击：方案→工站管理，工站/流程→配置，工具→调试 */
    void onTreeDouble(QTreeWidgetItem* item, int col);
    /** @brief 树单击选中 */
    void onTreeClicked(QTreeWidgetItem* item, int col);
    /** @brief 用户画完 ROI */
    void onRoi(const HsCommData::Graphic& g);
    /** @brief 打开详细调试窗 */
    void onOpenDebug();
    /** @brief 工具上移 */
    void onMoveUp();
    /** @brief 工具下移 */
    void onMoveDown();
    /** @brief 重命名 */
    void onRename();
    /** @brief 复制工具 */
    void onCopyTool();
    /** @brief 删除工具 */
    void onDeleteTool();
    /** @brief 树右键菜单 */
    void onTreeMenu(const QPoint& pos);
    /** @brief 算法库双击：向当前流程添加工具 */
    void onAlgLibDouble(QTreeWidgetItem* item, int col);
    /** @brief 画布点了工具 */
    void onCanvasTool(const QString& toolId);
    /** @brief 画布双击工具，打开该工具界面 */
    void onCanvasToolDouble(const QString& toolId);
    /** @brief 画布点了组 */
    void onCanvasGroup(const QString& groupId);
    /** @brief 画布右键 */
    void onCanvasMenu(const QString& id, int kind, const QPoint& globalPos);
    /** @brief 画布连线变化 */
    void onCanvasLinksChanged();
    /** @brief 详细调试窗改了 ROI */
    void onDebugRoiChanged();
    /** @brief 详细调试窗 draft 已提交到 formal */
    void onDebugDraftSaved();
    /** @brief 详细调试窗试跑完成，同步主界面结果与树着色 */
    void onDebugTestRun(const ToolOut& out);
    /** @brief 刷新上移/下移/删除按钮 */
    void updateToolButtons();

private:
    /** @brief 选中工具并刷新结果区 */
    void selectTool(const QString& toolId);
    /** @brief 设计态：展示整 JobCfg 运行结果（汇总各工具输出） */
    void showJobResult(const JobCfg* job, const ToolOut& jobOut);
    /** @brief 选中流程节点时刷新结果区（无单工具上下文） */
    void refreshJobResultPreview();
    /** @brief 刷新 JobCfg 画布 */
    void refreshCanvas();
    /** @brief 刷新右侧调试结果区 */
    void refreshResult(const ToolOut* runMeta = 0, bool afterJobRun = false);
    /** @brief 把当前工具 ROI 刷到主图 */
    void applyDrawingsToView();
    /** @brief 按流程缓存刷新资源树工具行 OK/NG 背景 */
    void refreshTreeRunStatus(const JobCfg* scopeJob);
    /** @brief 按绘制工具与 ROI 同步 Drawing 层 */
    void syncDrawingInteractionView();
    /** @brief 当前工具 ROI 变更后的公共收尾 */
    void afterRoiEdited();

    Session* m_session;                 ///< 会话
    Solution* m_sol;                    ///< 方案
    Station* m_station;                 ///< 工站运行实例，不拥有
    QTreeWidget* m_tree;                ///< 方案树（工站/流程）
    QTreeWidget* m_toolLib;             ///< 算法库（可注册工具）
    QLabel* m_solTitle;                 ///< 方案树标题
    QLabel* m_algoTitle;                ///< 算法库标题
    QLabel* m_canvasTitle;              ///< 画布标题
    JobFlowCanvasPanel* m_flowCanvas;   ///< 当前 JobCfg 画布（含连线写回）
    HsCtrl::HsViewShell* m_shell;      ///< 图像/点云壳
    HsCtrl::HsImageWindow* m_view;     ///< 图像窗（壳内）
    QLabel* m_status;                   ///< 状态/坐标
    QTableWidget* m_log;                ///< 日志表
    ToolRstPanel* m_result;          ///< 调试与结果显示
    ToolDebugHost* m_debug;             ///< 详细调试窗（含参数与 ROI）
    QPushButton* m_btnDebug;            ///< 详细调试
    QPushButton* m_btnRunTool;          ///< 运行本工具
    QPushButton* m_btnRunJob;           ///< 运行本流程
    ToolOut m_lastRun;                  ///< 最近一次运行摘要
    bool m_hasLastRun;                  ///< 是否已有 m_lastRun
    QString m_lastRunJobId;             ///< 最近一次跑流程的 JobCfg id（树着色范围）
    QString m_toolId;                   ///< 当前工具
    QString m_jobId;                    ///< 当前 JobCfg
    bool m_canUp;                       ///< 可上移
    bool m_canDown;                     ///< 可下移
    bool m_canEditTool;                 ///< 可改工具
};

}

#endif
