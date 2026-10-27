/**
 * @file JobCanvas.h
 * @brief JobCanvas头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOBCANVAS_H
#define VISION_JOBCANVAS_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QGraphicsView>
#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QVector>

class QGraphicsItem;
class QGraphicsEllipseItem;
class QGraphicsRectItem;
class QGraphicsPathItem;
class QGraphicsScene;
class QPainterPath;
class QWheelEvent;
class QMouseEvent;
class QContextMenuEvent;
class QKeyEvent;
class QResizeEvent;
class QScrollBar;

namespace Vision {

/** @brief 画布命中：空白、工具、组、数据边 */
enum CanvasHitKind {
    CanvasHitBlank = 0, ///< 空白或标题
    CanvasHitTool,      ///< 工具节点
    CanvasHitGroup,     ///< 并行/条件/序列组
    CanvasHitWire       ///< 端口连线
};

/**
 * @class CanvasWireRef
 * @brief 一条已画连线：路径图元与两端针脚键
 */
struct CanvasWireRef {
    QGraphicsPathItem* item; ///< 曲线
    QString fromKey;         ///< 起点针脚键
    QString toKey;           ///< 终点针脚键
    int linkIndex;           ///< job.links 下标
    CanvasWireRef() : item(nullptr), linkIndex(-1) {}
};

/**
 * @class CanvasOrderRef
 * @brief 一条执行顺序虚线箭头：上一节点底边中点到下一节点顶边中点（箭头指向下游）
 */
struct CanvasOrderRef {
    QGraphicsPathItem* item; ///< 虚线 + 终点箭头
    QGraphicsItem* from;     ///< 上一节点（工具或组框）
    QGraphicsItem* to;       ///< 下一节点
    CanvasOrderRef() : item(nullptr), from(nullptr), to(nullptr) {}
};

/**
 * @class JobCanvas
 * @brief 设计态流程画布：节点可拖、端口拖线、连线随节点更新
 */
class VISION_UI_API JobCanvas : public QGraphicsView {
    Q_OBJECT
public:
    /** @brief 空画布 */
    explicit JobCanvas(QWidget* parent = 0);
    /**
     * @brief 重画一个 JobCfg
     * @param selectedId 高亮的工具或组 id
     * @param selectedLinkIndex 高亮的数据连线索引；-1 表示无
     */
    void showJob(const JobCfg* job, const QString& selectedId, int selectedLinkIndex = -1);
    /** @brief 当前缩放（1 = 100%） */
    qreal zoomScale() const;
    /** @brief 缩放置回 100% */
    void resetZoom();
    /** @brief 最近一次 linkRouteChanged 的中间拐点（场景坐标） */
    QVector<QPointF> pendingRoutePoints() const;

signals:
    /** @brief 点了工具节点 */
    void toolClicked(const QString& toolId);
    /** @brief 双击工具节点，打开该工具界面 */
    void toolDoubleClicked(const QString& toolId);
    /** @brief 点了流程组节点 */
    void groupClicked(const QString& groupId);
    /**
     * @brief 右键菜单
     * @param id 工具或组 id；连线时为 links 下标字符串
     * @param kind CanvasHitKind
     */
    void canvasMenuRequested(const QString& id, int kind, const QPoint& globalPos);
    /** @brief 从输出端口拖到输入端口 */
    void linkPorts(const QString& fromId, const QString& fromPort,
                   const QString& toId, const QString& toPort);
    /** @brief 请求断开 job.links[index] */
    void unlinkRequested(int linkIndex);
    /**
     * @brief 用户拖完并行/条件组框
     * @param groupId 组 id
     * @param x 场景左上角 X
     * @param y 场景左上角 Y
     */
    void groupMoved(const QString& groupId, qreal x, qreal y);
    /**
     * @brief 用户拖完工具节点
     * @param toolId 工具 id
     * @param x 场景左上角 X
     * @param y 场景左上角 Y
     */
    void nodeMoved(const QString& toolId, qreal x, qreal y);
    /** @brief 点了数据连线 */
    void wireClicked(int linkIndex);
    /**
     * @brief 用户改完连线折线（拐点列表见 pendingRoutePoints）
     * @param linkIndex links 下标
     */
    void linkRouteChanged(int linkIndex);
    /**
     * @brief 用户请求恢复该连线自动路由
     * @param linkIndex links 下标
     */
    void linkRouteReset(int linkIndex);

protected:
    /** @brief 命中图元：选中、开始拖节点或拖线、平移（空白左键/中键） */
    void mousePressEvent(QMouseEvent* e);
    /** @brief 双击工具节点 */
    void mouseDoubleClickEvent(QMouseEvent* e);
    /** @brief 拖节点（连线跟随）、拖线预览或平移视图 */
    void mouseMoveEvent(QMouseEvent* e);
    /** @brief 结束拖节点、拖线或平移 */
    void mouseReleaseEvent(QMouseEvent* e);
    /** @brief 滚轮缩放 */
    void wheelEvent(QWheelEvent* e);
    /** @brief 右键发菜单信号 */
    void contextMenuEvent(QContextMenuEvent* e);
    /** @brief 数字 0 复位缩放 */
    void keyPressEvent(QKeyEvent* e);
    /** @brief 左侧滚动条布局 */
    void resizeEvent(QResizeEvent* e);

private slots:
    /** @brief 右侧垂直滚动条变化时同步左侧 */
    void onVertScroll(int value);
    /** @brief 左侧垂直滚动条变化时同步右侧 */
    void onLeftScroll(int value);

private:
    /** @brief 从点击处解析命中 */
    bool hitAt(const QPoint& viewPos, QString* id, int* kind, QString* port, int* dir) const;
    /** @brief 拖线时按距离找针脚（预览线在上层，不能用 itemAt） */
    bool hitPinAt(const QPoint& viewPos, QString* id, QString* port, int* dir) const;
    /** @brief 点击处的工具框（针脚不算） */
    QGraphicsItem* toolBoxAt(const QPoint& viewPos) const;
    /** @brief 点击处的并行/条件组框（空白区，不含工具；忽略连线） */
    QGraphicsItem* groupFrameAt(const QPoint& viewPos) const;
    /** @brief 场景点是否落在组内某工具框上 */
    bool scenePointOnGroupTool(QGraphicsRectItem* frame, const QPointF& sp) const;
    /** @brief 拖节点开始前记下自定义折线 */
    void captureNodeWireSnaps();
    /** @brief 节点移动后保持端口处的折线仍是横竖 */
    void applyNodeWireSnaps();
    /** @brief 按当前针脚位置重算数据连线 */
    void updateWires();
    /** @brief 收集点击处附近全部连线（用于叠线轮换） */
    void collectWireHitsAt(const QPoint& viewPos, QVector<int>* outIndices) const;
    /** @brief 按选中索引刷新连线粗细与拐点手柄 */
    void refreshWireSelectionVisual();
    /** @brief 更新选中连线的拐点/段手柄 */
    void updateWireHandles();
    /** @brief 计算一条连线路径；可选返回中间拐点 */
    QPainterPath wirePathForLink(int linkIndex, const QPointF& a, const QPointF& b,
                                 QVector<QPointF>* outMids = 0) const;
    /** @brief 取连线当前中间拐点（预览优先） */
    QVector<QPointF> midPointsForLink(int linkIndex, const QPointF& a, const QPointF& b) const;
    /** @brief 把自动绕障路径收成中间拐点并进入预览 */
    void beginCustomPreviewFromAuto(int linkIndex, const QPointF& a, const QPointF& b);
    /** @brief 按节点位置重算执行顺序虚线 */
    void updateOrderLines();
    /** @brief 结束拖节点或拖组，并把工具坐标写回 */
    void finishNodeMove();
    /** @brief 工具拖动后让祖先组框重新包住子项 */
    void refitGroupChain(QGraphicsItem* item);
    /** @brief 更新左侧滚动条位置与可见性 */
    void layoutLeftScrollBar();

    QGraphicsScene* m_scene;           ///< 节点场景
    qreal m_zoom;                      ///< 视图缩放
    bool m_panning;                    ///< 平移中（中键或空白左键）
    QScrollBar* m_vScrollLeft;         ///< 左侧垂直滚动条（与右侧同步）
    bool m_blockScrollSync;            ///< 滚动条双向同步锁
    QPoint m_panLast;                  ///< 上一平移点
    bool m_wiring;                     ///< 正在从输出端口拖线
    QString m_wireFromId;              ///< 拖线起点工具
    QString m_wireFromPort;            ///< 拖线起点端口
    PortData m_wireFromData;           ///< 拖线起点数据类型
    QPointF m_wireFromPos;             ///< 拖线起点场景坐标
    QGraphicsPathItem* m_tempWire;     ///< 拖线橡皮筋（与正式连线同一套曲线）
    QHash<QString, QGraphicsItem*> m_pinItems; ///< 针脚键到图元（圆/菱形/方）
    QVector<CanvasWireRef> m_wires;    ///< 当前 JobCfg 的数据连线
    QVector<CanvasOrderRef> m_orderLines; ///< 执行顺序虚线
    bool m_movingNode;                 ///< 正在拖工具节点
    bool m_movingGroup;                ///< 正在拖并行/条件组框
    bool m_moveArmed;                  ///< 已按下工具或组，等待超过阈值再拖
    QGraphicsItem* m_moveItem;         ///< 正在拖的工具框或组框
    QString m_moveId;                  ///< 正在拖的工具或组 id
    QPointF m_moveGrab;                ///< 按下时鼠标相对节点场景左上角的偏移
    QPoint m_movePressView;            ///< 按下时的视图坐标
    /** @brief 拖节点前记下的一条自定义折线 */
    struct NodeWireSnap {
        int linkIndex;                 ///< 连线下标
        QVector<QPointF> mids;         ///< 按下时的中间拐点
        QPointF pinA;                  ///< 按下时起点端口
        QPointF pinB;                  ///< 按下时终点端口
        bool srcHoriz;                 ///< 起点侧沿水平短桩
        bool dstHoriz;                 ///< 终点侧沿水平短桩
    };
    QVector<NodeWireSnap> m_nodeWireSnaps; ///< 拖节点期间的折线快照
    const JobCfg* m_job;                  ///< 当前显示的 JobCfg（不拥有）
    QHash<QString, QGraphicsItem*> m_toolBoxes;  ///< 工具框（几何命中，不受连线遮挡）
    QHash<QString, QGraphicsItem*> m_groupFrames; ///< 可拖动的并行/条件组框
    QString m_selectedGroupId;         ///< 当前选中的组 id
    int m_selectedLinkIndex;           ///< 选中的连线索引；-1 无
    QPoint m_wirePickPos;              ///< 上次点选连线处（叠线轮换）
    QVector<int> m_wirePickList;       ///< 上次点击命中的连线索引列表
    int m_wirePickIndex;               ///< 叠线轮换当前下标
    bool m_draggingWireHandle;         ///< 正在拖连线拐点或线段
    int m_dragLinkIndex;               ///< 被拖的连线索引
    int m_dragVertexIndex;             ///< 拖拐点时：中间点下标；拖段时 -1
    int m_dragSegIndex;                ///< 拖段时：整路径段下标
    bool m_dragIsSegment;              ///< true=拖整段，false=拖拐点
    QPointF m_dragPressScene;          ///< 拖线按下时的场景坐标
    QVector<QPointF> m_dragBaseMids;   ///< 按下时的中间拐点，拖动按位移改
    QVector<QGraphicsEllipseItem*> m_wireVertexHandles; ///< 拐点手柄
    QVector<QGraphicsRectItem*> m_wireSegHandles;       ///< 段中点手柄（拖段）
    bool m_previewRoute;               ///< 拖拽/编辑预览中
    int m_previewLinkIndex;            ///< 预览连线索引
    QVector<QPointF> m_previewMids;    ///< 预览中间拐点
    QVector<QPointF> m_pendingRoutePoints; ///< 写回用中间拐点
};

}

#endif