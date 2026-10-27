/**
 * @file MiscEditors.cpp
 * @brief MiscEditors实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditors.h"
#include "QtConv.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cstdio>

namespace Vision {

class MergeEditor : public ToolEditorBase {
public:
    explicit MergeEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_minA = new QSpinBox(this);
        m_minA->setRange(0, 100000000);
        m_maxC = new QSpinBox(this);
        m_maxC->setRange(0, 100000);
        QFormLayout* f = new QFormLayout(this);
        f->addRow(T("最小面积"), m_minA);
        f->addRow(T("最大数量(0不限)"), m_maxC);
        connect(m_minA, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_maxC, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        m_minA->setValue(m_tool->params.value("minArea").toInt());
        m_maxC->setValue(m_tool->params.value("maxCount").toInt());
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("minArea", m_minA->value());
        m_tool->params.insert("maxCount", m_maxC->value());
    }
    QSpinBox* m_minA;
    QSpinBox* m_maxC;
};

class JudgeEditor : public ToolEditorBase {
public:
    explicit JudgeEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_useOk = new QCheckBox(T("同时汇总上游工具 OK/NG"), this);
        m_useOk->setChecked(true);
        m_table = new QTableWidget(0, 4, this);
        m_table->setHorizontalHeaderLabels(QStringList() << T("源工具") << T("字段") << T("比较") << T("阈值"));
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        QPushButton* addBtn = new QPushButton(T("添加条件"), this);
        QPushButton* delBtn = new QPushButton(T("删除"), this);
        QHBoxLayout* btns = new QHBoxLayout();
        btns->addWidget(addBtn);
        btns->addWidget(delBtn);
        btns->addStretch(1);
        QLabel* tip = new QLabel(
            T("字段常用：score / x / y / angle / area / count / width / r / ok。比较：>= <= > < == !="), this);
        tip->setWordWrap(true);
        tip->setStyleSheet(QStringLiteral("color:#666;"));
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->addWidget(m_useOk);
        lay->addWidget(m_table, 1);
        lay->addLayout(btns);
        lay->addWidget(tip);
        connect(m_useOk, &QCheckBox::toggled, this, &JudgeEditor::onEdited);
        connect(addBtn, &QPushButton::clicked, this, &JudgeEditor::onAddTerm);
        connect(delBtn, &QPushButton::clicked, this, &JudgeEditor::onDelTerm);
        connect(m_table, SIGNAL(cellChanged(int,int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        m_block = true;
        m_useOk->setChecked(m_tool->params.value("useOkAnd").toBool(true));
        m_table->setRowCount(0);
        const int n = m_tool->params.value("termCount").toInt(0);
        for (int i = 0; i < n; ++i) {
            char idKey[32];
            char keyKey[32];
            char opKey[32];
            char valKey[32];
            sprintf_s(idKey, "term%d_id", i);
            sprintf_s(keyKey, "term%d_key", i);
            sprintf_s(opKey, "term%d_op", i);
            sprintf_s(valKey, "term%d_val", i);
            const ObjectId id = ObjectId::fromString(m_tool->params.value(idKey).toString());
            appendRow(id, qs(m_tool->params.value(keyKey).toString()),
                      qs(m_tool->params.value(opKey).toString()),
                      m_tool->params.value(valKey).toDouble(0.0));
        }
        // 兼容旧 expr：有文本且无条件时塞一行提示到空表
        if (n <= 0) {
            const QString old = qs(m_tool->params.value("expr").toString());
            if (!old.isEmpty()) {
                tipFromExpr(old);
            }
        }
        m_block = false;
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("useOkAnd", m_useOk->isChecked());
        const int oldN = m_tool->params.value("termCount").toInt(0);
        for (int i = 0; i < oldN; ++i) {
            char idKey[32];
            char keyKey[32];
            char opKey[32];
            char valKey[32];
            sprintf_s(idKey, "term%d_id", i);
            sprintf_s(keyKey, "term%d_key", i);
            sprintf_s(opKey, "term%d_op", i);
            sprintf_s(valKey, "term%d_val", i);
            m_tool->params.raw().erase(idKey);
            m_tool->params.raw().erase(keyKey);
            m_tool->params.raw().erase(opKey);
            m_tool->params.raw().erase(valKey);
        }
        const int rows = m_table->rowCount();
        int written = 0;
        for (int r = 0; r < rows; ++r) {
            QComboBox* src = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
            QComboBox* key = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
            QComboBox* op = qobject_cast<QComboBox*>(m_table->cellWidget(r, 2));
            QDoubleSpinBox* val = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r, 3));
            if (!src || !key || !op || !val) {
                continue;
            }
            const QString id = src->currentData().toString();
            if (id.isEmpty()) {
                continue;
            }
            char idKey[32];
            char keyKey[32];
            char opKey[32];
            char valKey[32];
            sprintf_s(idKey, "term%d_id", written);
            sprintf_s(keyKey, "term%d_key", written);
            sprintf_s(opKey, "term%d_op", written);
            sprintf_s(valKey, "term%d_val", written);
            m_tool->params.insert(idKey, ss(id));
            m_tool->params.insert(keyKey, ss(key->currentData().toString()));
            m_tool->params.insert(opKey, ss(op->currentData().toString()));
            m_tool->params.insert(valKey, val->value());
            ++written;
        }
        m_tool->params.insert("termCount", written);
        m_tool->params.insert("expr", std::string());
    }

private:
    void onAddTerm()
    {
        appendRow(ObjectId(), QStringLiteral("score"), QStringLiteral(">="), 0.5);
        onEdited();
    }
    void onDelTerm()
    {
        const int r = m_table->currentRow();
        if (r >= 0) {
            m_table->removeRow(r);
            onEdited();
        }
    }
    void tipFromExpr(const QString&)
    {
        // 旧自由表达式不再解析，改用表格条件
    }
    void fillSrcCombo(QComboBox* c, const ObjectId& selected)
    {
        c->clear();
        c->addItem(T("(选择工具)"), QString());
        if (!m_sol || !m_tool) {
            return;
        }
        JobCfg* job = m_sol->findJobCfgOfTool(m_tool->id, 0);
        if (!job) {
            return;
        }
        const std::vector<ObjectId> order = job->execOrder();
        for (int i = 0; i < static_cast<int>(order.size()); ++i) {
            if (order[static_cast<size_t>(i)] == m_tool->id) {
                break;
            }
            const ToolCfg* t = job->findTool(order[static_cast<size_t>(i)]);
            if (!t) {
                continue;
            }
            if (t->type == ToolType::Acquire || t->type == ToolType::Comm) {
                continue;
            }
            c->addItem(qs(t->name), qs(t->id));
        }
        const int idx = c->findData(qs(selected));
        c->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    void fillKeyCombo(QComboBox* c, const QString& selected)
    {
        c->clear();
        const char* keys[] = {
            "score", "x", "y", "angle", "area", "count", "width", "r", "ok", 0
        };
        for (int i = 0; keys[i]; ++i) {
            c->addItem(QLatin1String(keys[i]), QLatin1String(keys[i]));
        }
        const int idx = c->findData(selected);
        c->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    void fillOpCombo(QComboBox* c, const QString& selected)
    {
        c->clear();
        const char* ops[] = { ">=", "<=", ">", "<", "==", "!=", 0 };
        for (int i = 0; ops[i]; ++i) {
            c->addItem(QLatin1String(ops[i]), QLatin1String(ops[i]));
        }
        const int idx = c->findData(selected.isEmpty() ? QStringLiteral(">=") : selected);
        c->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    void appendRow(const ObjectId& src, const QString& key, const QString& op, double val)
    {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        QComboBox* srcBox = new QComboBox(m_table);
        fillSrcCombo(srcBox, src);
        QComboBox* keyBox = new QComboBox(m_table);
        fillKeyCombo(keyBox, key.isEmpty() ? QStringLiteral("score") : key);
        QComboBox* opBox = new QComboBox(m_table);
        fillOpCombo(opBox, op);
        QDoubleSpinBox* valBox = new QDoubleSpinBox(m_table);
        valBox->setRange(-1e12, 1e12);
        valBox->setDecimals(4);
        valBox->setValue(val);
        m_table->setCellWidget(r, 0, srcBox);
        m_table->setCellWidget(r, 1, keyBox);
        m_table->setCellWidget(r, 2, opBox);
        m_table->setCellWidget(r, 3, valBox);
        connect(srcBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &JudgeEditor::onEdited);
        connect(keyBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &JudgeEditor::onEdited);
        connect(opBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &JudgeEditor::onEdited);
        connect(valBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this, &JudgeEditor::onEdited);
    }

    QCheckBox* m_useOk;
    QTableWidget* m_table;
};

class PlcEditor : public ToolEditorBase {
public:
    explicit PlcEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_host = new QLineEdit(this);
        m_port = new QSpinBox(this);
        m_port->setRange(0, 65535);
        QFormLayout* f = new QFormLayout(this);
        f->addRow(T("主机"), m_host);
        f->addRow(T("端口"), m_port);
        connect(m_host, SIGNAL(editingFinished()), this, SLOT(onEdited()));
        connect(m_port, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        m_host->setText(qs(m_tool->params.value("host").toString()));
        m_port->setValue(m_tool->params.value("port").toInt());
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("host", ss(m_host->text()));
        m_tool->params.insert("port", m_port->value());
    }
    QLineEdit* m_host;
    QSpinBox* m_port;
};

/** @brief 未登记专用页时的占位编辑器 */
class GenericEditor : public ToolEditorBase {
public:
    explicit GenericEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        QLabel* tip = new QLabel(T("该工具暂无专用参数页。"), this);
        tip->setWordWrap(true);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->addWidget(tip);
        lay->addStretch(1);
    }

protected:
    void loadFromTool() {}
    void saveToTool() {}
};

// 结果合并参数页
ToolEditorBase* createMergeEditor(QWidget* parent) { return new MergeEditor(parent); }
// 逻辑判断参数页
ToolEditorBase* createJudgeEditor(QWidget* parent) { return new JudgeEditor(parent); }
// PLC 通信参数页
ToolEditorBase* createPlcEditor(QWidget* parent) { return new PlcEditor(parent); }
// 无专用页时的占位参数页
ToolEditorBase* createGenericEditor(QWidget* parent) { return new GenericEditor(parent); }

}
