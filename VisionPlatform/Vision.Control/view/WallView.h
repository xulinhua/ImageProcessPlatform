/**
 * @file WallView.h
 * @brief 监控墙。每格一张卡片：标题栏、子码流画面、状态边框、悬停按钮；空格可拖入设备
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_WALL_VIEW_H
#define VISION_CONTROL_WALL_VIEW_H

#include <QImage>
#include <QString>
#include <QWidget>
#include <functional>
#include <vector>

namespace Vision {

/**
 * @class WallView
 * @brief 监控墙：按行列画当前页的格子，鼠标操作通过回调交给 ControlWindow
 * @note 只负责绘制和命中判断，不知道分页；格子内容由 setCells() 整体替换
 */
class WallView : public QWidget {
public:
    /** @brief 悬停按钮：只看、操作（打开会话并申请）、文件传输、全屏 */
    enum Action { ActView = 0, ActOperate = 1, ActFile = 2, ActFull = 3 };

    /** @brief 一个格子的显示内容，key 为空表示空格 */
    struct Cell {
        QString key;        ///< 设备库 key，空为空格
        QString name;       ///< 显示名
        QString share;      ///< 共享种类标签（桌面/窗口），在线时显示
        QString fps;        ///< 帧率文字，如「12 fps」
        QString badge;      ///< 操作中徽标文字，空不显示
        QString lastSeen;   ///< 离线时显示的最后在线时间，可空
        QString station;    ///< 工站状态，空不显示
        bool alarm;         ///< 工站连续 NG，右下角红字
        QImage image;       ///< 最近一帧，离线时为灰度图
        bool online;        ///< 在线
        bool operating;     ///< 本端在操作，边框变蓝
        /** @brief 离线、未操作 */
        Cell() : online(false), operating(false) {}
    };

    static const char* const kMime;     ///< 拖设备用的 MIME 类型，数据为设备 key（UTF-8）

    /** @brief 默认 3x3，接受拖放 */
    explicit WallView(QWidget* parent = 0);

    /************************************************** 设置 **************************************************/
    /** @brief 设行列数，小于 1 按 1 */
    void setLayoutCount(int rows, int cols);
    /** @brief 替换当前页格子，下标即格子序号（行优先） */
    void setCells(const std::vector<Cell>& cells);
    /** @brief 悬停按钮或双击在线格子的回调：设备 key、Action */
    void setOnAction(const std::function<void(const QString&, int)>& action);
    /** @brief 设备拖到格子上的回调：设备 key、格子序号 */
    void setOnDrop(const std::function<void(const QString&, int)>& drop);
    /** @brief 双击空格的回调 */
    void setOnEmpty(const std::function<void()>& empty);
    /** @brief 在有设备的格子上右键的回调：设备 key、全局坐标 */
    void setOnContext(const std::function<void(const QString&, const QPoint&)>& context);

protected:
    /************************************************** 事件 **************************************************/
    /** @brief 逐格绘制：有设备画卡片，否则画空格 */
    void paintEvent(QPaintEvent* event) override;
    /** @brief 更新悬停格子和按钮，设光标与提示 */
    void mouseMoveEvent(QMouseEvent* event) override;
    /** @brief 左键点在悬停按钮上时触发动作（文件按钮除外） */
    void mousePressEvent(QMouseEvent* event) override;
    /** @brief 双击空格添加设备，双击在线格子打开会话 */
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    /** @brief 清除悬停状态 */
    void leaveEvent(QEvent* event) override;
    /** @brief 有设备的格子弹右键菜单回调 */
    void contextMenuEvent(QContextMenuEvent* event) override;
    /** @brief 只接受 kMime 拖入 */
    void dragEnterEvent(QDragEnterEvent* event) override;
    /** @brief 高亮拖放目标格子 */
    void dragMoveEvent(QDragMoveEvent* event) override;
    /** @brief 取消高亮 */
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    /** @brief 放下时回调 setOnDrop */
    void dropEvent(QDropEvent* event) override;

private:
    /************************************************** 几何 **************************************************/
    /** @brief 第 index 格的矩形，四周留 12、格间距 10 */
    QRect cellRect(int index) const;
    /** @brief 格子里标题栏（高 30）以下的画面区 */
    QRect bodyRect(const QRect& cell) const;
    /** @brief 第 action 个悬停按钮的矩形，在画面区右上角横排 */
    QRect actionRect(const QRect& cell, int action) const;
    /** @brief 点落在哪一格，都不在返回 -1 */
    int cellAt(const QPoint& point) const;
    /** @brief 点落在在线格子的哪个悬停按钮上，没有返回 -1 */
    int actionAt(int cell, const QPoint& point) const;

    /************************************************** 绘制 **************************************************/
    /** @brief 画一张设备卡片：画面、离线遮罩、标题栏、徽标、悬停按钮、状态边框 */
    void paintCell(QPainter& painter, int index);
    /** @brief 画空格虚线框和「拖入设备」提示；drop 为真时高亮 */
    void paintEmpty(QPainter& painter, const QRect& box, bool drop);

    int m_rows;                                                     ///< 行数，>= 1
    int m_cols;                                                     ///< 列数，>= 1
    int m_hover;                                                    ///< 悬停格子序号，-1 无
    int m_hoverAction;                                              ///< 悬停按钮 Action，-1 无
    int m_drop;                                                     ///< 拖放目标格子序号，-1 无
    std::vector<Cell> m_cells;                                      ///< 当前页格子，可能少于行×列
    std::function<void(const QString&, int)> m_action;              ///< 动作回调，可空
    std::function<void(const QString&, int)> m_dropTo;              ///< 拖放回调，可空
    std::function<void()> m_empty;                                  ///< 双击空格回调，可空
    std::function<void(const QString&, const QPoint&)> m_context;   ///< 右键回调，可空
};

}

#endif
