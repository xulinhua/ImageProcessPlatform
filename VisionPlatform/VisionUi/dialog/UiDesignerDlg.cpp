/**
 * @file UiDesignerDlg.cpp
 * @brief UiDesignerDlg ??
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UiDesignerDlg.h"
#include "ObjectId.h"
#include "QtConv.h"
#include "Theme.h"
#include "UiText.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Vision {

QString UiDesignerDlg::kindTitle(UiWidgetKind k)
{
    switch (k) {
    case UiWImageView: return T("图像窗");
    case UiWLabel: return T("标签");
    case UiWValue: return T("数值");
    case UiWOkNg: return T("OK/NG");
    case UiWRunOnce: return T("单次运行");
    case UiWLoop: return T("循环运行");
    case UiWButton: return T("按钮");
    default: return T("控件");
    }
}

UiDesignerDlg::UiDesignerDlg(Solution* sol, QWidget* parent)
    : QDialog(parent)
    , m_sol(sol)
    , m_canvas(0)
    , m_palette(0)
    , m_pageName(0)
    , m_enabled(0)
    , m_text(0)
    , m_dataPath(0)
    , m_action(0)
    , m_x(0)
    , m_y(0)
    , m_w(0)
    , m_h(0)
    , m_blockProp(false)
{
    setWindowTitle(T("UI 设计器"));
    setModal(true);
    resize(1100, 700);
    ensurePage();

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(T("页面名称"), this));
    m_pageName = new QLineEdit(qs(m_page.name), this);
    top->addWidget(m_pageName, 1);
    m_enabled = new QCheckBox(T("运行态启用"), this);
    m_enabled->setChecked(m_page.enabled);
    top->addWidget(m_enabled);
    QPushButton* delBtn = new QPushButton(T("删除控件"), this);
    top->addWidget(delBtn);
    lay->addLayout(top);

    QHBoxLayout* body = new QHBoxLayout();

    m_palette = new QListWidget(this);
    m_palette->setFixedWidth(140);
    m_palette->addItem(kindTitle(UiWImageView));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWImageView));
    m_palette->addItem(kindTitle(UiWLabel));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWLabel));
    m_palette->addItem(kindTitle(UiWValue));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWValue));
    m_palette->addItem(kindTitle(UiWOkNg));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWOkNg));
    m_palette->addItem(kindTitle(UiWRunOnce));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWRunOnce));
    m_palette->addItem(kindTitle(UiWLoop));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWLoop));
    m_palette->addItem(kindTitle(UiWButton));
    m_palette->item(m_palette->count() - 1)->setData(Qt::UserRole, static_cast<int>(UiWButton));
    QVBoxLayout* left = new QVBoxLayout();
    left->addWidget(new QLabel(T("控件板"), this));
    left->addWidget(m_palette, 1);
    QPushButton* addBtn = new QPushButton(T("添加到画布"), this);
    left->addWidget(addBtn);
    body->addLayout(left);

    m_canvas = new QWidget(this);
    m_canvas->setMinimumSize(m_page.width, m_page.height);
    m_canvas->setStyleSheet(QString::fromLatin1("background:#2B2F36;border:1px solid #4A4F57;"));
    body->addWidget(m_canvas, 1);

    QWidget* prop = new QWidget(this);
    prop->setFixedWidth(220);
    QFormLayout* form = new QFormLayout(prop);
    m_text = new QLineEdit(prop);
    m_dataPath = new QLineEdit(prop);
    m_action = new QLineEdit(prop);
    m_x = new QSpinBox(prop); m_x->setRange(0, 4000);
    m_y = new QSpinBox(prop); m_y->setRange(0, 4000);
    m_w = new QSpinBox(prop); m_w->setRange(16, 4000);
    m_h = new QSpinBox(prop); m_h->setRange(16, 4000);
    form->addRow(T("文字"), m_text);
    form->addRow(QString::fromLatin1("dataPath"), m_dataPath);
    form->addRow(QString::fromLatin1("action"), m_action);
    form->addRow(QString::fromLatin1("X"), m_x);
    form->addRow(QString::fromLatin1("Y"), m_y);
    form->addRow(QString::fromLatin1("W"), m_w);
    form->addRow(QString::fromLatin1("H"), m_h);
    QVBoxLayout* right = new QVBoxLayout();
    right->addWidget(new QLabel(T("属性"), this));
    right->addWidget(prop);
    right->addStretch();
    body->addLayout(right);

    lay->addLayout(body, 1);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(box);

    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddWidget()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onRemoveWidget()));
    connect(m_text, SIGNAL(textEdited(QString)), this, SLOT(onPropChanged()));
    connect(m_dataPath, SIGNAL(textEdited(QString)), this, SLOT(onPropChanged()));
    connect(m_action, SIGNAL(textEdited(QString)), this, SLOT(onPropChanged()));
    connect(m_x, SIGNAL(valueChanged(int)), this, SLOT(onPropChanged()));
    connect(m_y, SIGNAL(valueChanged(int)), this, SLOT(onPropChanged()));
    connect(m_w, SIGNAL(valueChanged(int)), this, SLOT(onPropChanged()));
    connect(m_h, SIGNAL(valueChanged(int)), this, SLOT(onPropChanged()));
    connect(box, SIGNAL(accepted()), this, SLOT(onSave()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    rebuildCanvas();
    enableDialogResize(this);
}

void UiDesignerDlg::ensurePage()
{
    if (m_sol && !m_sol->uiPages.empty()) {
        m_page = m_sol->uiPages[0];
        return;
    }
    m_page = UiPage();
    m_page.id = ObjectId::create().toString();
    m_page.name = "Operator";
    m_page.width = 800;
    m_page.height = 600;
    m_page.enabled = true;
}

void UiDesignerDlg::rebuildCanvas()
{
    const QList<QWidget*> kids = m_canvas->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 0; i < kids.size(); ++i) {
        kids[i]->deleteLater();
    }
    m_proxy.clear();
    for (size_t i = 0; i < m_page.widgets.size(); ++i) {
        const UiWidget& w = m_page.widgets[i];
        QPushButton* proxy = new QPushButton(m_canvas);
        proxy->setGeometry(w.x, w.y, w.w, w.h);
        QString title = qs(w.text);
        if (title.isEmpty()) {
            title = kindTitle(w.kind);
        }
        proxy->setText(title);
        proxy->setProperty("wid", qs(w.id));
        if (qs(w.id) == m_selectedId) {
            proxy->setStyleSheet(QString::fromLatin1("border:2px solid #1677FF;background:#3A4048;color:#fff;"));
        } else {
            proxy->setStyleSheet(QString::fromLatin1("border:1px solid #8C8C8C;background:#3A4048;color:#fff;"));
        }
        proxy->show();
        connect(proxy, SIGNAL(clicked()), this, SLOT(onSelectWidget()));
        m_proxy.insert(qs(w.id), proxy);
    }
}

int UiDesignerDlg::selectedIndex() const
{
    if (m_selectedId.isEmpty()) {
        return -1;
    }
    const std::string id = ss(m_selectedId);
    for (size_t i = 0; i < m_page.widgets.size(); ++i) {
        if (m_page.widgets[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UiDesignerDlg::loadProps()
{
    m_blockProp = true;
    const int idx = selectedIndex();
    if (idx < 0) {
        m_text->clear();
        m_dataPath->clear();
        m_action->clear();
        m_x->setValue(0);
        m_y->setValue(0);
        m_w->setValue(120);
        m_h->setValue(32);
        m_blockProp = false;
        return;
    }
    const UiWidget& w = m_page.widgets[static_cast<size_t>(idx)];
    m_text->setText(qs(w.text));
    m_dataPath->setText(qs(w.dataPath));
    m_action->setText(qs(w.action));
    m_x->setValue(w.x);
    m_y->setValue(w.y);
    m_w->setValue(w.w);
    m_h->setValue(w.h);
    m_blockProp = false;
}

void UiDesignerDlg::onAddWidget()
{
    QListWidgetItem* it = m_palette->currentItem();
    if (!it) {
        return;
    }
    UiWidget w;
    w.id = ObjectId::create().toString();
    w.kind = static_cast<UiWidgetKind>(it->data(Qt::UserRole).toInt());
    w.x = 40 + static_cast<int>(m_page.widgets.size()) * 12;
    w.y = 40 + static_cast<int>(m_page.widgets.size()) * 12;
    w.w = (w.kind == UiWImageView) ? 320 : 120;
    w.h = (w.kind == UiWImageView) ? 240 : 36;
    w.text = ss(kindTitle(w.kind));
    if (w.kind == UiWRunOnce) {
        w.action = "runOnce";
    } else if (w.kind == UiWLoop) {
        w.action = "loop";
    } else if (w.kind == UiWButton) {
        w.action = "stop";
    }
    m_page.widgets.push_back(w);
    m_selectedId = qs(w.id);
    rebuildCanvas();
    loadProps();
}

void UiDesignerDlg::onSelectWidget()
{
    QObject* s = sender();
    if (!s) {
        return;
    }
    m_selectedId = s->property("wid").toString();
    rebuildCanvas();
    loadProps();
}

void UiDesignerDlg::onPropChanged()
{
    if (m_blockProp) {
        return;
    }
    const int idx = selectedIndex();
    if (idx < 0) {
        return;
    }
    UiWidget& w = m_page.widgets[static_cast<size_t>(idx)];
    w.text = ss(m_text->text());
    w.dataPath = ss(m_dataPath->text());
    w.action = ss(m_action->text());
    w.x = m_x->value();
    w.y = m_y->value();
    w.w = m_w->value();
    w.h = m_h->value();
    rebuildCanvas();
}

void UiDesignerDlg::onRemoveWidget()
{
    const int idx = selectedIndex();
    if (idx < 0) {
        return;
    }
    m_page.widgets.erase(m_page.widgets.begin() + idx);
    m_selectedId.clear();
    rebuildCanvas();
    loadProps();
}

void UiDesignerDlg::onSave()
{
    if (!m_sol) {
        reject();
        return;
    }
    m_page.name = ss(m_pageName->text().trimmed());
    if (m_page.name.empty()) {
        m_page.name = "Operator";
    }
    m_page.enabled = m_enabled->isChecked();
    if (m_sol->uiPages.empty()) {
        m_sol->uiPages.push_back(m_page);
    } else {
        m_sol->uiPages[0] = m_page;
    }
    m_sol->dirty = true;
    accept();
}

}

#include "moc_UiDesignerDlg.cpp"
