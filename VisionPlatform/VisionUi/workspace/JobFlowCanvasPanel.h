/**
 * @file JobFlowCanvasPanel.h
 * @brief JobFlowCanvasPanel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOBFLOWCANVASPANEL_H
#define VISION_JOBFLOWCANVASPANEL_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QWidget>
#include <QString>

class QLabel;
class QPoint;

namespace Vision {

class JobCanvas;
class Session;
struct Solution;

/**
 * @class JobFlowCanvasPanel
 * @brief 流程画布面板：JobCanvas + 提示 + 连线/断线/节点坐标写回（主页与流程编辑共用）
 */
class VISION_UI_API JobFlowCanvasPanel : public QWidget {
    Q_OBJECT
public:
    /** @brief 构造流程画布面板；会话与方案不拥有 */
    JobFlowCanvasPanel(Session* session, Solution* solution, QWidget* parent = 0);

    /** @brief 底层画布视图 */
    JobCanvas* canvas() const;
    /** @brief 是否允许改连线与拖节点 */
    void setEditable(bool editable);
    /** @brief draft 模式：写回 JobCfg 草稿，不置 Solution::dirty */
    void setDraftMode(bool draft);
    /**
     * @brief 绑定当前 JobCfg（showJob 时也会更新）
     * @param job 不拥有；空则后续按 jobId / fallbackToolId 解析
     */
    void bindJob(JobCfg* job);
    /** @brief 固定 JobCfg id（流程编辑对话框） */
    void setJobId(const QString& jobId);
    /** @brief 找不到 jobId 时用工具反查 JobCfg（设计主页） */
    void setFallbackToolId(const QString& toolId);
    /** @brief 设置顶部提示文案；空则隐藏 */
    void setHintText(const QString& text);
    /**
     * @brief 刷新画布
     * @param selectedId 高亮工具或组 id
     */
    void showJob(const QString& selectedId = QString());
    /** @brief 缩放置回 100% */
    void resetZoom();
    /** @brief 断开指定下标的数据连线 */
    void unlinkLink(int linkIndex);
    /** @brief 恢复指定连线自动路由 */
    void resetLinkRoute(int linkIndex);

signals:
    /** @brief 点了工具节点 */
    void toolClicked(const QString& toolId);
    /** @brief 双击工具节点 */
    void toolDoubleClicked(const QString& toolId);
    /** @brief 点了流程组 */
    void groupClicked(const QString& groupId);
    /**
     * @brief 右键菜单
     * @param id 工具/组 id；连线时为 links 下标字符串
     */
    void canvasMenuRequested(const QString& id, int kind, const QPoint& globalPos);
    /** @brief 数据连线或断线已写回方案 */
    void linksChanged();
    /**
     * @brief 用户拖完工具节点
     * @param toolId 工具 id
     * @param x 场景左上角 X
     * @param y 场景左上角 Y
     */
    void nodeMoved(const QString& toolId, qreal x, qreal y);

private slots:
    /** @brief 画布拖端口连线 */
    void onLinkPorts(const QString& fromId, const QString& fromPort,
                     const QString& toId, const QString& toPort);
    /** @brief 画布断开数据线 */
    void onUnlink(int linkIndex);
    /** @brief 画布拖完并行/条件组坐标 */
    void onGroupMoved(const QString& groupId, qreal x, qreal y);
    /** @brief 画布拖完工具坐标 */
    void onNodeMoved(const QString& toolId, qreal x, qreal y);
    /** @brief 画布点了工具，清除连线选中 */
    void onToolClickedClearWire(const QString& toolId);
    /** @brief 画布点了连线 */
    void onWireClicked(int linkIndex);
    /** @brief 用户改完连线折线 */
    void onLinkRouteChanged(int linkIndex);
    /** @brief 恢复连线自动路由 */
    void onLinkRouteReset(int linkIndex);

private:
    /** @brief 解析当前 JobCfg */
    JobCfg* resolveJob() const;
    /** @brief 是否有方案编辑权限且允许编辑 */
    bool canEditLinks() const;

    Session* m_session;       ///< 会话（不拥有）
    Solution* m_sol;          ///< 方案（不拥有）
    JobCfg* m_job;               ///< 当前 JobCfg（不拥有）
    QString m_jobId;          ///< 固定 JobCfg id
    QString m_fallbackToolId; ///< 反查 JobCfg 的工具 id
    bool m_editable;          ///< 是否允许改连线
    bool m_draftMode;         ///< 是否 JobCfg 草稿模式
    int m_selectedLinkIndex;  ///< 当前选中的连线索引；-1 无
    QLabel* m_hint;           ///< 操作提示
    JobCanvas* m_canvas;      ///< 流程画布
};

}

#endif