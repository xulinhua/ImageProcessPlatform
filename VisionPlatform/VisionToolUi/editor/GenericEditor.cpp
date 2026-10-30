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
#include "Session.h"
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

// 一行参数：字段说明与对应控件
struct FieldRow {
    ParamField field;   ///< 工具壳给出的字段说明
    QWidget* editor;    ///< 取值控件；路径行为其中的 QLineEdit，不拥有
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
    // 按工具壳 paramFields 重建表单；没有字段时只放一行提示
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

    // 当前用户能否改该字段（按工具名、所属流程、工具 id 查权限）
    bool allowField(const std::string& key) const
    {
        if (!m_session || !m_tool) {
            return false;
        }
        std::string flowId;
        if (m_sol) {
            JobCfg* job = m_sol->findJobCfgOfTool(m_tool->id, 0);
            if (job) {
                flowId = job->id.toString();
            }
        }
        return m_session->allowToolField(m_tool->toolName, flowId, m_tool->id.toString(), key);
    }

    // 控件值写回参数；设备字段写 deviceId，下拉写当前显示文本
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
    // 删除全部行控件和布局项
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

    // 按字段类型建控件：布尔、整数、浮点、设备、候选下拉，其余为文本框（路径另带浏览按钮）
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
            // 取图、标定工具只列相机、文件夹、图像三类设备
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
                wrap->setEnabled(allowField(f.key));
                m_rows.push_back(row);
                m_lay->addWidget(mkParamCell(this, T(f.title.c_str()), wrap));
                m_holders.push_back(wrap);
                return;
            }
            editor = line;
        }
        row.editor = editor;
        if (editor) {
            editor->setEnabled(allowField(f.key));
        }
        m_rows.push_back(row);
        QWidget* cell = mkParamCell(this, T(f.title.c_str()), editor);
        m_lay->addWidget(cell);
        m_holders.push_back(cell);
    }

    QVBoxLayout* m_lay;                 ///< 表单纵向布局
    std::vector<FieldRow> m_rows;       ///< 各字段行，顺序同 paramFields
    std::vector<QWidget*> m_holders;    ///< 本页建的行容器，clearRows 时删除
};

// 按注册工具的参数说明生成参数页
ToolEditorBase* createGenericEditor(QWidget* parent)
{
    return new GenericEditor(parent);
}

}
