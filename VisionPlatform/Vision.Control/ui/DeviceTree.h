/**
 * @file DeviceTree.h
 * @brief 主窗口左侧：搜索、分组/最近连接/局域网发现三页、设备树
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROL_DEVICE_TREE_H
#define VISION_CONTROL_DEVICE_TREE_H

#include <QDateTime>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QWidget>
#include <functional>
#include <set>
#include <vector>

class QLineEdit;
class QPushButton;
class QTabBar;

namespace Vision {

/**
 * @class DeviceItemDelegate
 * @brief 分组行、设备行、发现行的画法
 * @note 行的内容全部放在下面的自定义数据角色里，由 DeviceTree 填写
 */
class DeviceItemDelegate : public QStyledItemDelegate {
public:
    /** @brief 条目上的自定义数据角色 */
    enum Role {
        RoleKind = Qt::UserRole,    ///< 行类型，取值见 Kind
        RoleKey,                    ///< 分组行为分组名（空为未分组），设备行为设备 key，发现行为发现列表下标（int）
        RoleTone,                   ///< 状态点颜色名：green、orange、red、gray（gray 视为离线）
        RoleMetaIcon,               ///< 右侧附注图标名，可空
        RoleMetaText,               ///< 右侧附注文字
        RoleMetaColor,              ///< 右侧附注颜色，空用次要文字色
        RoleCount,                  ///< 分组行的「在线/总数」文字
        RoleSearch                  ///< 搜索用的小写拼接文本：名称、设备 ID、地址
    };
    /** @brief 行类型：分组、设备、局域网发现项 */
    enum Kind { KindGroup = 0, KindDevice = 1, KindFound = 2 };

    /** @brief 无状态 */
    explicit DeviceItemDelegate(QObject* parent = 0);
    /** @brief 分组行画箭头、粗体名和计数；其他行画选中底、状态点、名称（超长省略）和右侧附注 */
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    /** @brief 固定行高 32 */
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

/**
 * @class DeviceTreeWidget
 * @brief 设备可拖到分组行或监控墙格子
 * @note 拖动数据用 WallView::kMime，内容为设备 key
 */
class DeviceTreeWidget : public QTreeWidget {
public:
    /** @brief 无表头、单选、不取焦点、自定义右键菜单 */
    explicit DeviceTreeWidget(QWidget* parent = 0);
    /** @brief 设备拖到分组上的回调：设备 key、目标分组名 */
    void setOnMove(const std::function<void(const QString&, const QString&)>& move);

protected:
    /** @brief 只有设备行能拖，带上设备 key */
    void startDrag(Qt::DropActions actions) override;
    /** @brief 只接受 kMime 拖入 */
    void dragEnterEvent(QDragEnterEvent* event) override;
    /** @brief 悬在任意行上都接受，放下时再换算到分组 */
    void dragMoveEvent(QDragMoveEvent* event) override;
    /** @brief 放到分组行或其下设备行时，回调移到该分组 */
    void dropEvent(QDropEvent* event) override;

private:
    std::function<void(const QString&, const QString&)> m_move;    ///< 移组回调，可空
};

/**
 * @class DeviceTree
 * @brief 设备侧栏：搜索框、三个页签（分组、最近连接、局域网发现）、树和底部按钮
 * @note 不持有设备库，行数据由 ControlWindow 定时 setRows()；结构不变时只就地刷新状态，避免折叠与选中闪动
 */
class DeviceTree : public QWidget {
public:
    /** @brief 一台设备在树上的显示数据 */
    struct Row {
        QString key;            ///< 设备 key
        QString name;           ///< 显示名
        QString group;          ///< 分组名，空或不在分组表里归入未分组
        QString address;        ///< 地址，用于搜索
        QString deviceId;       ///< 设备 ID，无 ID 的设备为空，用于搜索
        const char* tone;       ///< 状态点颜色名，静态字符串
        const char* metaIcon;   ///< 右侧附注图标名，可空，静态字符串
        QString metaText;       ///< 右侧附注文字
        QString metaColor;      ///< 右侧附注颜色，空用默认
        QDateTime lastOpened;   ///< 最近打开会话时间，无效则不进「最近连接」
        /** @brief 灰色、无图标 */
        Row() : tone("gray"), metaIcon(0) {}
    };

    /** @brief 局域网发现的一项 */
    struct Found {
        QString name;       ///< 被控端名称，可空
        QString address;    ///< 地址
        QString deviceId;   ///< 设备 ID，可空
        bool added;         ///< 已在设备库里
        /** @brief 未添加 */
        Found() : added(false) {}
    };

    /** @brief 搭建侧栏，接好搜索、页签、单击、双击、右键和新建分组 */
    explicit DeviceTree(QWidget* parent = 0);

    /************************************************** 数据 **************************************************/
    /** @brief 更新分组与设备行；结构签名变了才重建，否则就地刷新 */
    void setRows(const QStringList& groups, const std::vector<Row>& rows);
    /** @brief 更新发现列表与页签计数，当前在发现页时重建 */
    void setFound(const std::vector<Found>& found);
    /** @brief 记住并选中设备 key 对应的行 */
    void setSelected(const QString& key);
    /** @brief 最近单击选中的设备 key，可能已不在库里 */
    QString selected() const;
    /** @brief 当前页签：0 分组，1 最近连接，2 局域网发现 */
    int currentTab() const;

    /************************************************** 回调 **************************************************/
    /** @brief 双击设备行：设备 key */
    void setOnOpen(const std::function<void(const QString&)>& open);
    /** @brief 单击设备行：设备 key */
    void setOnSelect(const std::function<void(const QString&)>& select);
    /** @brief 右键：设备 key、分组名（二者至多一个非空，都空为空白处）、全局坐标 */
    void setOnContext(const std::function<void(const QString&, const QString&, const QPoint&)>& context);
    /** @brief 设备拖到分组：设备 key、分组名 */
    void setOnMove(const std::function<void(const QString&, const QString&)>& move);
    /** @brief 点「新建分组」 */
    void setOnNewGroup(const std::function<void()>& newGroup);
    /** @brief 点远程唤醒 */
    void setOnWake(const std::function<void()>& wake);
    /** @brief 双击发现项：发现列表下标 */
    void setOnAddFound(const std::function<void(int)>& addFound);
    /** @brief 切页签：页签下标 */
    void setOnTab(const std::function<void(int)>& tab);

private:
    /************************************************** 内部 **************************************************/
    /** @brief 按当前页签清空重建树，恢复选中并应用搜索 */
    void rebuild();
    /** @brief 结构不变时只更新设备行的状态和分组计数 */
    void refreshInPlace();
    /** @brief 按搜索框过滤：隐藏不匹配的行，分组下全不匹配时隐藏分组 */
    void applyFilter();
    /** @brief 树结构签名：页签、分组表、每行 key/名称/分组/最近打开时间 */
    QString signature() const;
    /** @brief 新建设备行，parent 为空时作为顶层 */
    QTreeWidgetItem* deviceItem(const Row& row, QTreeWidgetItem* parent);
    /** @brief 把 Row 写进条目的文字和数据角色 */
    void fillDevice(QTreeWidgetItem* item, const Row& row);

    QLineEdit* m_search;                ///< 搜索框
    QTabBar* m_tabs;                    ///< 页签：分组、最近连接、局域网发现
    DeviceTreeWidget* m_tree;           ///< 树
    QPushButton* m_newGroup;            ///< 新建分组按钮
    QPushButton* m_wake;                ///< 远程唤醒按钮（未实现，置灰）
    QStringList m_groups;               ///< 分组表，来自设备库
    std::vector<Row> m_rows;            ///< 全部设备行
    std::vector<Found> m_found;         ///< 局域网发现项，下标即发现行的 RoleKey
    std::set<QString> m_collapsed;      ///< 用户收起的分组名，重建时保持收起
    QString m_signature;                ///< 上次重建时的结构签名
    QString m_selected;                 ///< 选中的设备 key
    std::function<void(const QString&)> m_open;                                     ///< 双击设备回调，可空
    std::function<void(const QString&)> m_select;                                   ///< 单击设备回调，可空
    std::function<void(const QString&, const QString&, const QPoint&)> m_context;   ///< 右键回调，可空
    std::function<void()> m_newGroupFn;                                             ///< 新建分组回调，可空
    std::function<void()> m_wakeFn;                                                 ///< 远程唤醒回调，可空
    std::function<void(int)> m_addFound;                                            ///< 添加发现项回调，可空
    std::function<void(int)> m_tab;                                                 ///< 切页签回调，可空
};

}

#endif
