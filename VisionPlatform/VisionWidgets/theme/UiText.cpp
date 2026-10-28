/**
 * @file UiText.cpp
 * @brief UiText实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UiText.h"


// 按已绑定的 vpTr 键刷新控件/动作文案
void applyTrKey(QObject* o)
{
    if (!o) {
        return;
    }
    const QByteArray k = o->property("vpTr").toByteArray();
    if (k.isEmpty()) {
        return;
    }
    const QString text = T(k.constData());
    if (QAction* a = qobject_cast<QAction*>(o)) {
        a->setText(text);
        return;
    }
    if (QAbstractButton* b = qobject_cast<QAbstractButton*>(o)) {
        b->setText(text);
        return;
    }
    if (QLabel* l = qobject_cast<QLabel*>(o)) {
        l->setText(text);
        return;
    }
    if (QGroupBox* g = qobject_cast<QGroupBox*>(o)) {
        g->setTitle(text);
        return;
    }
    if (QMenu* m = qobject_cast<QMenu*>(o)) {
        m->setTitle(text);
        return;
    }
}

// 递归刷新本对象树中绑定了 vpTr 的文案
void retranslateBound(QObject* root)
{
    if (!root) {
        return;
    }
    applyTrKey(root);
    const QObjectList kids = root->children();
    for (int i = 0; i < kids.size(); ++i) {
        retranslateBound(kids.at(i));
    }
}


