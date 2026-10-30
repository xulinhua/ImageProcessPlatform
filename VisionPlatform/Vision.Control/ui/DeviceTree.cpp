/**
 * @file DeviceTree.cpp
 * @brief 设备树
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeviceTree.h"
#include "RemoteTheme.h"
#include "WallView.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QTabBar>
#include <QVBoxLayout>
#include <algorithm>

namespace Vision {

DeviceItemDelegate::DeviceItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize DeviceItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    (void)option;
    (void)index;
    return QSize(200, 32);
}

void DeviceItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const int kind = index.data(RoleKind).toInt();
    const QRect r = option.rect.adjusted(6, 0, -6, 0);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QFont font = option.font;
    font.setPixelSize(13);

    // 分组行：展开箭头、粗体组名、灰色计数
    if (kind == KindGroup) {
        if (hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(RemoteTheme::kPage));
            painter->drawRoundedRect(r, 4, 4);
        }
        const bool open = option.state & QStyle::State_Open;
        painter->drawPixmap(r.left() + 6, r.top() + 9, RemoteTheme::pixmap(open ? "down" : "right", QColor(RemoteTheme::kMuted), 14, painter->device()->devicePixelRatioF()));
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(QColor(RemoteTheme::kText));
        const QString name = index.data(Qt::DisplayRole).toString();
        const int nameW = QFontMetrics(font).horizontalAdvance(name);
        painter->drawText(QRect(r.left() + 26, r.top(), nameW + 4, r.height()), Qt::AlignVCenter | Qt::AlignLeft, name);
        font.setBold(false);
        font.setPixelSize(12);
        painter->setFont(font);
        painter->setPen(QColor(RemoteTheme::kMuted));
        painter->drawText(QRect(r.left() + 32 + nameW, r.top(), 60, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
            index.data(RoleCount).toString());
        painter->restore();
        return;
    }

    // 设备行和发现行：选中/悬停底色，状态点；分组下的行多缩进
    const bool nested = index.parent().isValid();
    if (selected || hover) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor("#E6F4FF") : QColor("#F5F5F5"));
        painter->drawRoundedRect(r, 4, 4);
    }
    const int left = r.left() + (nested ? 24 : 8);
    const QString tone = index.data(RoleTone).toString();
    QColor dot(RemoteTheme::kSuccess);
    if (tone == "gray") {
        dot = QColor("#BFBFBF");
    } else if (tone == "orange") {
        dot = QColor(RemoteTheme::kWarning);
    } else if (tone == "red") {
        dot = QColor(RemoteTheme::kDanger);
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(dot);
    painter->drawEllipse(QRectF(left, r.top() + 12, 8, 8));

    // 右侧附注靠右对齐，名称占剩余宽度
    const QString metaText = index.data(RoleMetaText).toString();
    const QString metaIcon = index.data(RoleMetaIcon).toString();
    const QString metaColor = index.data(RoleMetaColor).toString();
    QFont small = font;
    small.setPixelSize(11);
    const int metaTextW = QFontMetrics(small).horizontalAdvance(metaText);
    const int metaW = metaTextW + (metaIcon.isEmpty() ? 0 : 18);
    const QColor metaPen = metaColor.isEmpty() ? QColor(RemoteTheme::kMuted) : QColor(metaColor);
    int x = r.right() - 8 - metaW;
    if (!metaIcon.isEmpty()) {
        painter->drawPixmap(x, r.top() + 9, RemoteTheme::pixmap(metaIcon.toLatin1().constData(), metaPen, 13, painter->device()->devicePixelRatioF()));
        x += 18;
    }
    painter->setFont(small);
    painter->setPen(metaPen);
    painter->drawText(QRect(x, r.top(), metaTextW + 2, r.height()), Qt::AlignVCenter | Qt::AlignLeft, metaText);

    painter->setFont(font);
    QColor text(RemoteTheme::kText);
    if (selected) {
        text = QColor("#1677FF");
    } else if (tone == "gray") {
        text = QColor(RemoteTheme::kMuted);
    }
    painter->setPen(text);
    const QRect nameRect(left + 16, r.top(), r.right() - 12 - metaW - (left + 16), r.height());
    painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
        QFontMetrics(font).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, nameRect.width()));
    painter->restore();
}

DeviceTreeWidget::DeviceTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setIndentation(0);
    setMouseTracking(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setFocusPolicy(Qt::NoFocus);
    setExpandsOnDoubleClick(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void DeviceTreeWidget::setOnMove(const std::function<void(const QString&, const QString&)>& move)
{
    m_move = move;
}

void DeviceTreeWidget::startDrag(Qt::DropActions actions)
{
    (void)actions;
    QTreeWidgetItem* item = currentItem();
    if (!item || item->data(0, DeviceItemDelegate::RoleKind).toInt() != DeviceItemDelegate::KindDevice) {
        return;
    }
    QMimeData* mime = new QMimeData();
    mime->setData(WallView::kMime, item->data(0, DeviceItemDelegate::RoleKey).toString().toUtf8());
    QDrag* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

void DeviceTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(WallView::kMime)) {
        event->acceptProposedAction();
    }
}

void DeviceTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    if (item && event->mimeData()->hasFormat(WallView::kMime)) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void DeviceTreeWidget::dropEvent(QDropEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    if (!item || !m_move) {
        return;
    }
    if (item->data(0, DeviceItemDelegate::RoleKind).toInt() != DeviceItemDelegate::KindGroup && item->parent()) {
        item = item->parent();
    }
    if (item->data(0, DeviceItemDelegate::RoleKind).toInt() != DeviceItemDelegate::KindGroup) {
        return;
    }
    m_move(QString::fromUtf8(event->mimeData()->data(WallView::kMime)), item->data(0, DeviceItemDelegate::RoleKey).toString());
    event->acceptProposedAction();
}

DeviceTree::DeviceTree(QWidget* parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_tabs(new QTabBar(this))
    , m_tree(new DeviceTreeWidget(this))
    , m_newGroup(RemoteTheme::button(QString::fromUtf8("新建分组"), "plus", 0, this))
    , m_wake(RemoteTheme::button(QString::fromUtf8("远程唤醒"), "zap", 0, this))
{
    // 控件初始化；远程唤醒未实现，置灰
    setObjectName("side");
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(270);
    m_search->setPlaceholderText(QString::fromUtf8("搜索名称 / 设备 ID / IP"));
    m_search->addAction(RemoteTheme::icon("search", QColor(RemoteTheme::kMuted), 14), QLineEdit::LeadingPosition);
    m_search->setClearButtonEnabled(true);
    m_tabs->addTab(QString::fromUtf8("分组"));
    m_tabs->addTab(QString::fromUtf8("最近连接"));
    m_tabs->addTab(QString::fromUtf8("局域网发现"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    m_tree->setItemDelegate(new DeviceItemDelegate(m_tree));
    m_newGroup->setProperty("size", "sm");
    m_wake->setProperty("size", "sm");
    m_wake->setToolTip(QString::fromUtf8("向选中设备的 MAC 发魔术包"));

    // 布局：搜索、页签、树、底部按钮
    QWidget* tabLine = new QWidget(this);
    tabLine->setStyleSheet("border-bottom:1px solid #F0F0F0;");
    QHBoxLayout* tabRow = new QHBoxLayout(tabLine);
    tabRow->setContentsMargins(14, 0, 0, 0);
    tabRow->addWidget(m_tabs);
    tabRow->addStretch(1);

    QWidget* foot = new QWidget(this);
    foot->setStyleSheet("border-top:1px solid #F0F0F0;");
    QHBoxLayout* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(10, 10, 10, 10);
    footRow->setSpacing(8);
    footRow->addWidget(m_newGroup, 1);
    footRow->addWidget(m_wake, 1);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    QHBoxLayout* searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(10, 10, 10, 6);
    searchRow->addWidget(m_search);
    root->addLayout(searchRow);
    root->addWidget(tabLine);
    root->addSpacing(6);
    root->addWidget(m_tree, 1);
    root->addWidget(foot);

    // 搜索即时过滤，切页签重建；单击分组行切换展开并记住收起状态，单击设备行记选中
    QObject::connect(m_search, &QLineEdit::textChanged, [this](const QString&) { applyFilter(); });
    QObject::connect(m_tabs, &QTabBar::currentChanged, [this](int index) {
        rebuild();
        if (m_tab) {
            m_tab(index);
        }
    });
    QObject::connect(m_tree, &QTreeWidget::itemClicked, [this](QTreeWidgetItem* item, int) {
        const int kind = item->data(0, DeviceItemDelegate::RoleKind).toInt();
        if (kind == DeviceItemDelegate::KindGroup) {
            item->setExpanded(!item->isExpanded());
            const QString group = item->data(0, DeviceItemDelegate::RoleKey).toString();
            if (item->isExpanded()) {
                m_collapsed.erase(group);
            } else {
                m_collapsed.insert(group);
            }
            item->setSelected(false);
            return;
        }
        if (kind == DeviceItemDelegate::KindDevice) {
            m_selected = item->data(0, DeviceItemDelegate::RoleKey).toString();
            if (m_select) {
                m_select(m_selected);
            }
        }
    });
    // 双击设备打开会话，双击发现项加入设备库；右键按行类型分派
    QObject::connect(m_tree, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item, int) {
        const int kind = item->data(0, DeviceItemDelegate::RoleKind).toInt();
        if (kind == DeviceItemDelegate::KindDevice && m_open) {
            m_open(item->data(0, DeviceItemDelegate::RoleKey).toString());
        } else if (kind == DeviceItemDelegate::KindFound && m_addFound) {
            m_addFound(item->data(0, DeviceItemDelegate::RoleKey).toInt());
        }
    });
    QObject::connect(m_tree, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_tree->itemAt(pos);
        if (!m_context) {
            return;
        }
        const QPoint global = m_tree->viewport()->mapToGlobal(pos);
        if (!item) {
            m_context(QString(), QString(), global);
            return;
        }
        const int kind = item->data(0, DeviceItemDelegate::RoleKind).toInt();
        const QString key = item->data(0, DeviceItemDelegate::RoleKey).toString();
        if (kind == DeviceItemDelegate::KindGroup) {
            m_context(QString(), key, global);
        } else if (kind == DeviceItemDelegate::KindDevice) {
            m_context(key, QString(), global);
        }
    });
    QObject::connect(m_newGroup, &QPushButton::clicked, [this]() {
        if (m_newGroupFn) {
            m_newGroupFn();
        }
    });
    QObject::connect(m_wake, &QPushButton::clicked, [this]() {
        if (m_wakeFn) {
            m_wakeFn();
        }
    });
}

void DeviceTree::setOnOpen(const std::function<void(const QString&)>& open)
{
    m_open = open;
}

void DeviceTree::setOnSelect(const std::function<void(const QString&)>& select)
{
    m_select = select;
}

void DeviceTree::setOnContext(const std::function<void(const QString&, const QString&, const QPoint&)>& context)
{
    m_context = context;
}

void DeviceTree::setOnMove(const std::function<void(const QString&, const QString&)>& move)
{
    m_tree->setOnMove(move);
}

void DeviceTree::setOnNewGroup(const std::function<void()>& newGroup)
{
    m_newGroupFn = newGroup;
}

void DeviceTree::setOnWake(const std::function<void()>& wake)
{
    m_wakeFn = wake;
}

void DeviceTree::setOnAddFound(const std::function<void(int)>& addFound)
{
    m_addFound = addFound;
}

void DeviceTree::setOnTab(const std::function<void(int)>& tab)
{
    m_tab = tab;
}

int DeviceTree::currentTab() const
{
    return m_tabs->currentIndex();
}

QString DeviceTree::selected() const
{
    return m_selected;
}

void DeviceTree::setSelected(const QString& key)
{
    m_selected = key;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        QTreeWidgetItem* item = *it;
        if (item->data(0, DeviceItemDelegate::RoleKind).toInt() == DeviceItemDelegate::KindDevice
            && item->data(0, DeviceItemDelegate::RoleKey).toString() == key) {
            m_tree->setCurrentItem(item);
            return;
        }
        ++it;
    }
}

void DeviceTree::setRows(const QStringList& groups, const std::vector<Row>& rows)
{
    m_groups = groups;
    m_rows = rows;
    if (signature() != m_signature) {
        rebuild();
    } else {
        refreshInPlace();
    }
}

void DeviceTree::setFound(const std::vector<Found>& found)
{
    m_found = found;
    m_tabs->setTabText(2, found.empty() ? QString::fromUtf8("局域网发现") : QString::fromUtf8("局域网发现 %1").arg(found.size()));
    if (m_tabs->currentIndex() == 2) {
        rebuild();
    }
}

QString DeviceTree::signature() const
{
    QString out = QString::number(m_tabs->currentIndex()) + "|" + m_groups.join("\x1f") + "|";
    for (size_t i = 0; i < m_rows.size(); ++i) {
        out += m_rows[i].key + "\x1e" + m_rows[i].name + "\x1e" + m_rows[i].group + "\x1e"
            + m_rows[i].lastOpened.toString(Qt::ISODate) + "\x1f";
    }
    return out;
}

void DeviceTree::fillDevice(QTreeWidgetItem* item, const Row& row)
{
    item->setText(0, row.name);
    item->setData(0, DeviceItemDelegate::RoleKind, DeviceItemDelegate::KindDevice);
    item->setData(0, DeviceItemDelegate::RoleKey, row.key);
    item->setData(0, DeviceItemDelegate::RoleTone, QString::fromLatin1(row.tone));
    item->setData(0, DeviceItemDelegate::RoleMetaIcon, row.metaIcon ? QString::fromLatin1(row.metaIcon) : QString());
    item->setData(0, DeviceItemDelegate::RoleMetaText, row.metaText);
    item->setData(0, DeviceItemDelegate::RoleMetaColor, row.metaColor);
    item->setData(0, DeviceItemDelegate::RoleSearch, (row.name + " " + row.deviceId + " " + row.address).toLower());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
}

QTreeWidgetItem* DeviceTree::deviceItem(const Row& row, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
    fillDevice(item, row);
    return item;
}

void DeviceTree::rebuild()
{
    m_signature = signature();
    m_tree->clear();
    const int tab = m_tabs->currentIndex();
    // 分组页：有未归组设备或没有分组时追加「未分组」
    if (tab == 0) {
        QStringList groups = m_groups;
        bool loose = false;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].group.isEmpty() || !groups.contains(m_rows[i].group)) {
                loose = true;
            }
        }
        if (loose || groups.isEmpty()) {
            groups.append(QString());
        }
        for (int g = 0; g < groups.size(); ++g) {
            const QString group = groups.at(g);
            QTreeWidgetItem* head = new QTreeWidgetItem(m_tree);
            head->setText(0, group.isEmpty() ? QString::fromUtf8("未分组") : group);
            head->setData(0, DeviceItemDelegate::RoleKind, DeviceItemDelegate::KindGroup);
            head->setData(0, DeviceItemDelegate::RoleKey, group);
            head->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
            int total = 0;
            int online = 0;
            for (size_t i = 0; i < m_rows.size(); ++i) {
                const Row& row = m_rows[i];
                const bool mine = group.isEmpty() ? (row.group.isEmpty() || !m_groups.contains(row.group)) : row.group == group;
                if (!mine) {
                    continue;
                }
                deviceItem(row, head);
                total += 1;
                if (std::string(row.tone) != "gray") {
                    online += 1;
                }
            }
            head->setData(0, DeviceItemDelegate::RoleCount, QString("%1/%2").arg(online).arg(total));
            head->setExpanded(m_collapsed.find(group) == m_collapsed.end());
        }
    } else if (tab == 1) {
        // 最近连接页：按最近打开时间倒序，附注换成打开时间
        std::vector<Row> recent;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].lastOpened.isValid()) {
                recent.push_back(m_rows[i]);
            }
        }
        std::sort(recent.begin(), recent.end(), [](const Row& a, const Row& b) { return a.lastOpened > b.lastOpened; });
        for (size_t i = 0; i < recent.size(); ++i) {
            Row row = recent[i];
            row.metaIcon = 0;
            row.metaText = row.lastOpened.date() == QDate::currentDate()
                ? row.lastOpened.toString("HH:mm") : row.lastOpened.toString("MM-dd HH:mm");
            row.metaColor.clear();
            deviceItem(row, 0);
        }
    } else {
        // 局域网发现页：RoleKey 存下标，已添加的灰显
        for (size_t i = 0; i < m_found.size(); ++i) {
            const Found& found = m_found[i];
            QTreeWidgetItem* item = new QTreeWidgetItem(m_tree);
            item->setText(0, found.name.isEmpty() ? found.address : found.name);
            item->setData(0, DeviceItemDelegate::RoleKind, DeviceItemDelegate::KindFound);
            item->setData(0, DeviceItemDelegate::RoleKey, static_cast<int>(i));
            item->setData(0, DeviceItemDelegate::RoleTone, found.added ? QString("gray") : QString("green"));
            item->setData(0, DeviceItemDelegate::RoleMetaText,
                found.added ? QString::fromUtf8("已添加") : found.address + QString::fromUtf8(" · 双击添加"));
            item->setData(0, DeviceItemDelegate::RoleMetaColor, found.added ? QString() : QString("#389E0D"));
            item->setData(0, DeviceItemDelegate::RoleSearch, (found.name + " " + found.deviceId + " " + found.address).toLower());
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
    }
    setSelected(m_selected);
    applyFilter();
}

void DeviceTree::refreshInPlace()
{
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        QTreeWidgetItem* item = *it;
        const int kind = item->data(0, DeviceItemDelegate::RoleKind).toInt();
        if (kind == DeviceItemDelegate::KindDevice) {
            const QString key = item->data(0, DeviceItemDelegate::RoleKey).toString();
            for (size_t i = 0; i < m_rows.size(); ++i) {
                if (m_rows[i].key != key) {
                    continue;
                }
                Row row = m_rows[i];
                if (m_tabs->currentIndex() == 1) {
                    row.metaIcon = 0;
                    row.metaText = item->data(0, DeviceItemDelegate::RoleMetaText).toString();
                    row.metaColor.clear();
                }
                fillDevice(item, row);
                break;
            }
        } else if (kind == DeviceItemDelegate::KindGroup) {
            int total = item->childCount();
            int online = 0;
            for (int c = 0; c < total; ++c) {
                if (item->child(c)->data(0, DeviceItemDelegate::RoleTone).toString() != "gray") {
                    online += 1;
                }
            }
            item->setData(0, DeviceItemDelegate::RoleCount, QString("%1/%2").arg(online).arg(total));
        }
        ++it;
    }
    m_tree->viewport()->update();
}

void DeviceTree::applyFilter()
{
    const QString query = m_search->text().trimmed().toLower();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* top = m_tree->topLevelItem(i);
        if (top->data(0, DeviceItemDelegate::RoleKind).toInt() == DeviceItemDelegate::KindGroup) {
            int shown = 0;
            for (int c = 0; c < top->childCount(); ++c) {
                QTreeWidgetItem* child = top->child(c);
                const bool hit = query.isEmpty() || child->data(0, DeviceItemDelegate::RoleSearch).toString().contains(query);
                child->setHidden(!hit);
                shown += hit ? 1 : 0;
            }
            top->setHidden(!query.isEmpty() && shown == 0);
        } else {
            top->setHidden(!query.isEmpty() && !top->data(0, DeviceItemDelegate::RoleSearch).toString().contains(query));
        }
    }
}

}
