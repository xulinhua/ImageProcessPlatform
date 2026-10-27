/**
 * @file ToolParamUi.cpp
 * @brief ToolParamUi实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolParamUi.h"
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Vision {

namespace {

// 输入控件：固定 28px 高，横向可扩展
static void styleParamInput(QWidget* w)
{
    if (!w) {
        return;
    }
    w->setFixedHeight(28);
    QSizePolicy sp = w->sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Expanding);
    sp.setVerticalPolicy(QSizePolicy::Fixed);
    w->setSizePolicy(sp);
}

// 标签/块：纵向不参与拉伸
static void sealVertical(QWidget* w)
{
    if (!w) {
        return;
    }
    QSizePolicy sp = w->sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Expanding);
    sp.setVerticalPolicy(QSizePolicy::Fixed);
    w->setSizePolicy(sp);
}

}

// 参数页根样式
void initToolParamEditor(QWidget* editor)
{
    if (!editor) {
        return;
    }
    editor->setObjectName(QLatin1String("toolDebugParam"));
    sealVertical(editor);
}

// 步进框统一样式
static void styleSpinBox(QAbstractSpinBox* spin)
{
    if (!spin) {
        return;
    }
    if (spin->objectName().isEmpty()) {
        spin->setObjectName(QLatin1String("toolDebugParamSpin"));
    }
    spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    spin->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    styleParamInput(spin);
}

// 下拉框统一样式
static void styleComboBox(QComboBox* combo)
{
    if (!combo) {
        return;
    }
    if (combo->objectName().isEmpty()
        || combo->objectName() == QLatin1String("toolDebugCombo")) {
        combo->setObjectName(QLatin1String("toolDebugParamCombo"));
    }
    styleParamInput(combo);
}

// 为页内控件补 objectName
void applyParamControlStyle(QWidget* root)
{
    if (!root) {
        return;
    }
    const QList<QSpinBox*> spins = root->findChildren<QSpinBox*>();
    for (int i = 0; i < spins.size(); ++i) {
        styleSpinBox(spins.at(i));
    }
    const QList<QDoubleSpinBox*> dspins = root->findChildren<QDoubleSpinBox*>();
    for (int i = 0; i < dspins.size(); ++i) {
        styleSpinBox(dspins.at(i));
    }
    const QList<QComboBox*> combos = root->findChildren<QComboBox*>();
    for (int i = 0; i < combos.size(); ++i) {
        styleComboBox(combos.at(i));
    }
    const QList<QCheckBox*> checks = root->findChildren<QCheckBox*>();
    for (int i = 0; i < checks.size(); ++i) {
        QCheckBox* c = checks.at(i);
        if (c->objectName().isEmpty()) {
            c->setObjectName(QLatin1String("toolDebugParamCheck"));
        }
        c->setFixedHeight(28);
        sealVertical(c);
    }
    const QList<QLabel*> labels = root->findChildren<QLabel*>();
    for (int i = 0; i < labels.size(); ++i) {
        QLabel* lab = labels.at(i);
        if (lab->objectName().isEmpty()) {
            lab->setObjectName(QLatin1String("toolDebugParamLabel"));
        }
        lab->setFixedHeight(18);
        sealVertical(lab);
    }
    const QList<QLineEdit*> lines = root->findChildren<QLineEdit*>();
    for (int i = 0; i < lines.size(); ++i) {
        QLineEdit* line = lines.at(i);
        if (line->objectName() == QLatin1String("toolDebugParamLine")) {
            styleParamInput(line);
        }
    }
}

// 按当前宽度重算参数页高度，纵向锁死为内容高
void finalizeParamEditorLayout(QWidget* editor)
{
    if (!editor) {
        return;
    }
    QVBoxLayout* lay = qobject_cast<QVBoxLayout*>(editor->layout());
    if (!lay) {
        return;
    }
    applyParamControlStyle(editor);
    lay->setAlignment(Qt::AlignTop);
    lay->setSizeConstraint(QLayout::SetMinimumSize);

    const QList<QWidget*> cells = editor->findChildren<QWidget*>(QLatin1String("toolDebugParamCell"));
    for (int i = 0; i < cells.size(); ++i) {
        sealVertical(cells.at(i));
    }
    const QList<QWidget*> rows = editor->findChildren<QWidget*>(QLatin1String("toolDebugParamRow"));
    for (int i = 0; i < rows.size(); ++i) {
        sealVertical(rows.at(i));
    }
    // 非 cell/row 的顶层块（如按钮）也锁高度
    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem* item = lay->itemAt(i);
        if (!item) {
            continue;
        }
        QWidget* w = item->widget();
        if (!w) {
            continue;
        }
        sealVertical(w);
        if (w->minimumHeight() != w->maximumHeight() || w->maximumHeight() >= QWIDGETSIZE_MAX) {
            const int wh = w->sizeHint().height();
            if (wh > 0) {
                w->setFixedHeight(wh);
            }
        }
    }

    lay->activate();
    const int h = lay->totalSizeHint().height();
    if (h > 0) {
        editor->setFixedHeight(h);
    }
    sealVertical(editor);
}

// 参数标签
QLabel* mkParamLabel(QWidget* parent, const QString& text)
{
    QLabel* lab = new QLabel(text, parent);
    lab->setObjectName(QLatin1String("toolDebugParamLabel"));
    lab->setFixedHeight(18);
    sealVertical(lab);
    return lab;
}

// 整型步进框
QSpinBox* mkParamSpin(QWidget* parent, int lo, int hi)
{
    QSpinBox* s = new QSpinBox(parent);
    s->setRange(lo, hi);
    styleSpinBox(s);
    return s;
}

// 浮点步进框
QDoubleSpinBox* mkParamDSpin(QWidget* parent, double lo, double hi, int dec, double step)
{
    QDoubleSpinBox* s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi);
    s->setDecimals(dec);
    s->setSingleStep(step);
    styleSpinBox(s);
    return s;
}

// 下拉框
QComboBox* mkParamCombo(QWidget* parent)
{
    QComboBox* c = new QComboBox(parent);
    styleComboBox(c);
    return c;
}

// 勾选框
QCheckBox* mkParamCheck(QWidget* parent, const QString& text)
{
    QCheckBox* c = new QCheckBox(text, parent);
    c->setObjectName(QLatin1String("toolDebugParamCheck"));
    c->setFixedHeight(28);
    sealVertical(c);
    return c;
}

// 标签在上、控件在下
QWidget* mkParamCell(QWidget* parent, const QString& label, QWidget* control)
{
    QWidget* cell = new QWidget(parent);
    cell->setObjectName(QLatin1String("toolDebugParamCell"));
    QVBoxLayout* lay = new QVBoxLayout(cell);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->setAlignment(Qt::AlignTop);
    lay->addWidget(mkParamLabel(cell, label), 0, Qt::AlignTop);
    lay->addWidget(control, 0, Qt::AlignTop);
    // 18 + 4 + 28
    cell->setFixedHeight(50);
    sealVertical(cell);
    return cell;
}

// 一行两列参数
QWidget* mkParamRow2(QWidget* parent,
                     const QString& label1, QWidget* control1,
                     const QString& label2, QWidget* control2,
                     int hSpacing)
{
    QWidget* row = new QWidget(parent);
    row->setObjectName(QLatin1String("toolDebugParamRow"));
    QHBoxLayout* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(hSpacing);
    h->setAlignment(Qt::AlignTop);
    h->addWidget(mkParamCell(row, label1, control1), 1, Qt::AlignTop);
    h->addWidget(mkParamCell(row, label2, control2), 1, Qt::AlignTop);
    row->setFixedHeight(50);
    sealVertical(row);
    return row;
}

// 自上而下参数栈
QVBoxLayout* mkParamStack(QWidget* host, int spacing)
{
    QVBoxLayout* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(spacing);
    lay->setAlignment(Qt::AlignTop);
    return lay;
}

// 向参数栈追加一行
void addParamStackItem(QVBoxLayout* lay, QWidget* block)
{
    if (!lay || !block) {
        return;
    }
    sealVertical(block);
    lay->addWidget(block, 0, Qt::AlignTop);
}

}
