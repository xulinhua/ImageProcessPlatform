/**
 * @file GenericEditor.cpp
 * @brief GenericEditor实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditorBase.h"
#include "ToolBase.h"
#include "ToolParamUi.h"
#include "QtConv.h"
#include "UiText.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <vector>

namespace Vision {

namespace {

struct FieldRow {
    ParamField field;
    QWidget* editor;
    FieldRow() : editor(nullptr) {}
};

} // namespace

/**
 * @class GenericEditor
 * @brief 按工具壳 paramFields 生成参数表单
 */
class GenericEditor : public ToolEditorBase {
public:
    explicit GenericEditor(QWidget* parent)
        : ToolEditorBase(parent)
        , m_lay(nullptr)
    {
        m_lay = new QVBoxLayout(this);
        m_lay->setContentsMargins(0, 0, 0, 0);
    }

protected:
    void loadFromTool()
    {
        clearRows();
        if (!m_tool || !m_lay) {
            return;
        }
        ToolBase* shell = ToolRegistry::instance().find(m_tool->toolName);
        std::vector<ParamField> fields;
        if (shell) {
            fields = shell->paramFields();
        }
        if (fields.empty()) {
            QLabel* tip = new QLabel(T("该工具没有参数。"), this);
            tip->setWordWrap(true);
            m_lay->addWidget(tip);
            m_holders.push_back(tip);
            m_lay->addStretch(1);
            return;
        }
        for (size_t i = 0; i < fields.size(); ++i) {
            addRow(fields[i]);
        }
        m_lay->addStretch(1);
        applyParamControlStyle(this);
    }

    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const ParamField& f = m_rows[i].field;
            QWidget* w = m_rows[i].editor;
            if (f.type == ParamFieldType::Device) {
                QComboBox* box = qobject_cast<QComboBox*>(w);
                if (box) {
                    m_tool->deviceId = ObjectId::fromString(ss(box->currentData().toString()));
                }
                continue;
            }
            if (f.type == ParamFieldType::Bool) {
                QCheckBox* box = qobject_cast<QCheckBox*>(w);
                if (box) {
                    m_tool->params.insert(f.key, box->isChecked());
                }
            } else if (f.type == ParamFieldType::Int) {
                QSpinBox* box = qobject_cast<QSpinBox*>(w);
                if (box) {
                    m_tool->params.insert(f.key, box->value());
                }
            } else if (f.type == ParamFieldType::Double) {
                QDoubleSpinBox* box = qobject_cast<QDoubleSpinBox*>(w);
                if (box) {
                    m_tool->params.insert(f.key, box->value());
                }
            } else {
                QLineEdit* line = qobject_cast<QLineEdit*>(w);
                QComboBox* box = qobject_cast<QComboBox*>(w);
                if (line) {
                    m_tool->params.insert(f.key, ss(line->text()));
                } else if (box) {
                    m_tool->params.insert(f.key, ss(box->currentText()));
                }
            }
        }
    }

private:
    void clearRows()
    {
        m_rows.clear();
        for (int i = 0; i < static_cast<int>(m_holders.size()); ++i) {
            m_lay->removeWidget(m_holders[static_cast<size_t>(i)]);
            if (m_holders[static_cast<size_t>(i)]) {
                delete m_holders[static_cast<size_t>(i)];
                m_holders[static_cast<size_t>(i)] = nullptr;
            }
        }
        m_holders.clear();
        while (m_lay->count() > 0) {
            QLayoutItem* item = m_lay->takeAt(0);
            if (item) {
                delete item;
                item = nullptr;
            } else {
                break;
            }
        }
    }

    void addRow(const ParamField& f)
    {
        FieldRow row;
        row.field = f;
        QWidget* editor = nullptr;
        if (f.type == ParamFieldType::Bool) {
            QCheckBox* box = mkParamCheck(this, QString());
            box->setChecked(m_tool->params.value(f.key).toBool(f.def.toBool()));
            connect(box, SIGNAL(stateChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else if (f.type == ParamFieldType::Int) {
            const int lo = f.hasMin ? static_cast<int>(f.minValue) : -1000000000;
            const int hi = f.hasMax ? static_cast<int>(f.maxValue) : 1000000000;
            QSpinBox* box = mkParamSpin(this, lo, hi);
            box->setValue(m_tool->params.value(f.key).toInt(f.def.toInt()));
            connect(box, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else if (f.type == ParamFieldType::Double) {
            const double lo = f.hasMin ? f.minValue : -1e12;
            const double hi = f.hasMax ? f.maxValue : 1e12;
            QDoubleSpinBox* box = mkParamDSpin(this, lo, hi, 4, 0.1);
            box->setValue(m_tool->params.value(f.key).toDouble(f.def.toDouble()));
            connect(box, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
            editor = box;
        } else if (f.type == ParamFieldType::Device) {
            QComboBox* box = mkParamCombo(this);
            box->addItem(T("（无）"), QString());
            if (m_sol) {
                for (int i = 0; i < static_cast<int>(m_sol->inventory.size()); ++i) {
                    const HardwareDevice& d = m_sol->inventory[static_cast<size_t>(i)];
            if ((m_tool->type == ToolType::Acquire || m_tool->type == ToolType::Calib)
                && d.kind != DeviceCamera && d.kind != DeviceFolder && d.kind != DeviceImage) {
                        continue;
                    }
                    QString label = qs(d.name);
                    if (m_tool->type == ToolType::Acquire) {
                        label += QLatin1String(" [") + QString::fromUtf8(deviceKindTitle(d.kind).c_str()) + QLatin1Char(']');
                    }
                    box->addItem(label, qs(d.id));
                }
            }
            const QString cur = qs(m_tool->deviceId);
            const int idx = box->findData(cur);
            box->setCurrentIndex(idx >= 0 ? idx : 0);
            connect(box, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else if (!f.choices.empty()) {
            QComboBox* box = mkParamCombo(this);
            const std::string cur = m_tool->params.value(f.key).toString();
            const std::string def = f.def.toString();
            int pick = 0;
            for (size_t i = 0; i < f.choices.size(); ++i) {
                box->addItem(T(f.choices[i].c_str()));
                if (f.choices[i] == cur || (cur.empty() && f.choices[i] == def)) {
                    pick = static_cast<int>(i);
                }
            }
            box->setCurrentIndex(pick);
            connect(box, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else {
            QLineEdit* line = new QLineEdit(this);
            const std::string cur = m_tool->params.value(f.key).toString();
            line->setText(qs(cur.empty() ? f.def.toString() : cur));
            connect(line, SIGNAL(editingFinished()), this, SLOT(onEdited()));
            if (f.type == ParamFieldType::Path) {
                QWidget* wrap = new QWidget(this);
                QHBoxLayout* h = new QHBoxLayout(wrap);
                h->setContentsMargins(0, 0, 0, 0);
                h->addWidget(line, 1);
                QPushButton* browse = new QPushButton(T("浏览"), wrap);
                h->addWidget(browse);
                connect(browse, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
                        this, [this, line](bool) {
                    const QString path = QFileDialog::getOpenFileName(this, T("选择文件"), line->text());
                    if (!path.isEmpty()) {
                        line->setText(path);
                        onEdited();
                    }
                });
                editor = wrap;
                row.editor = line;
                m_rows.push_back(row);
                m_lay->addWidget(mkParamCell(this, T(f.title.c_str()), wrap));
                m_holders.push_back(wrap);
                return;
            }
            editor = line;
        }
        row.editor = editor;
        m_rows.push_back(row);
        QWidget* cell = mkParamCell(this, T(f.title.c_str()), editor);
        m_lay->addWidget(cell);
        m_holders.push_back(cell);
    }

    QVBoxLayout* m_lay;
    std::vector<FieldRow> m_rows;
    std::vector<QWidget*> m_holders;
};

// 按注册工具的参数说明生成参数页
ToolEditorBase* createGenericEditor(QWidget* parent)
{
    return new GenericEditor(parent);
}

}
