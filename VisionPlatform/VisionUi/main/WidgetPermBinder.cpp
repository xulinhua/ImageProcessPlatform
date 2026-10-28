/**
 * @file WidgetPermBinder.cpp
 * @brief WidgetPermBinder实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "WidgetPermBinder.h"
#include "Session.h"
#include <QAction>
#include <QWidget>

namespace Vision {

// 无权限则禁用（不隐藏，避免布局跳动）
void WidgetPermBinder::bindWidget(QWidget* w, Perm perm)
{
    if (!w) {
        return;
    }
    Item it;
    it.widget = w;
    it.perm = perm;
    m_items.push_back(it);
}

// 无权限则禁用（不隐藏，避免布局跳动）
void WidgetPermBinder::bindAction(QAction* a, Perm perm)
{
    if (!a) {
        return;
    }
    Item it;
    it.action = a;
    it.perm = perm;
    m_items.push_back(it);
}

// 无权限则禁用（不隐藏，避免布局跳动）
void WidgetPermBinder::apply(const Session& session) const
{
    for (size_t i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        const bool ok = session.has(it.perm);
        if (it.widget) {
            it.widget->setEnabled(ok);
        }
        if (it.action) {
            it.action->setEnabled(ok);
        }
    }
}

}
