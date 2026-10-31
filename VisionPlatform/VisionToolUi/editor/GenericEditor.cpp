/**
 * @file GenericEditor.cpp
 * @brief 按工具壳 cfg_param / cfg_teach / cfg_output 生成参数页（示教/运行/输出标签）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditorBase.h"
#include "ToolBase.h"
#include "ToolRegistry.h"
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
#include <QTabWidget>
#include <QVBoxLayout>
#include <vector>

namespace Vision {

/**
 * @class GenericEditor
 * @brief 按工具壳 param 说明生成表单；有示教字段时分「示教 / 参数」页，并附输出标签页
 */
class GenericEditor : public ToolEditorBase {
public:
    explicit GenericEditor(QWidget* parent)
        : ToolEditorBase(parent)
        , m_tabs(nullptr)
        , m_runHost(nullptr)
        , m_teachHost(nullptr)
        , m_outHost(nullptr)
        , m_runLay(nullptr)
        , m_teachLay(nullptr)
        , m_outLay(nullptr)
    {
        QVBoxLayout* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        m_tabs = new QTabWidget(this);
        m_tabs->setObjectName(QLatin1String("toolDebugParamTabs"));
        root->addWidget(m_tabs);

        m_runHost = new QWidget(m_tabs);
        m_runLay = new QVBoxLayout(m_runHost);
        m_runLay->setContentsMargins(0, 8, 0, 0);
        m_runLay->setSpacing(8);

        m_teachHost = new QWidget(m_tabs);
        m_teachLay = new QVBoxLayout(m_teachHost);
        m_teachLay->setContentsMargins(0, 8, 0, 0);
        m_teachLay->setSpacing(8);

        m_outHost = new QWidget(m_tabs);
        m_outLay = new QVBoxLayout(m_outHost);
        m_outLay->setContentsMargins(0, 8, 0, 0);
        m_outLay->setSpacing(6);
    }

protected:
    // 按 cfg_param / cfg_teach / cfg_output 重建；示教空则不建示教页
    void loadFromTool()
    {
        clearPage(m_runLay, &m_runRows, &m_runHolders);
        clearPage(m_teachLay, &m_teachRows, &m_teachHolders);
        clearOutPage();
        while (m_tabs->count() > 0) {
            m_tabs->removeTab(0);
        }
        if (!m_tool) {
            return;
        }
        ToolBase* shell = ToolRegistry::instance().find(m_tool->toolName);
        std::vector<ParamField> runFields;
        std::vector<ParamField> teachFields;
        OutputSpec outSpec;
        if (shell) {
            runFields = shell->cfg_param();
            teachFields = shell->cfg_teach();
            outSpec = shell->cfg_output();
            if (runFields.empty() && teachFields.empty()) {
                runFields = shell->paramFields();
            }
        }

        const bool hasTeach = !teachFields.empty();
        if (hasTeach) {
            m_tabs->addTab(m_teachHost, T("示教"));
            for (size_t i = 0; i < teachFields.size(); ++i) {
                addRow(teachFields[i], true, m_teachLay, &m_teachRows, &m_teachHolders);
            }
            m_teachLay->addStretch(1);
        }
        m_tabs->addTab(m_runHost, hasTeach ? T("参数") : T("参数"));
        if (runFields.empty()) {
            QLabel* tip = new QLabel(T("该工具没有参数。"), m_runHost);
            tip->setWordWrap(true);
            m_runLay->addWidget(tip);
            m_runHolders.push_back(tip);
        } else {
            for (size_t i = 0; i < runFields.size(); ++i) {
                addRow(runFields[i], false, m_runLay, &m_runRows, &m_runHolders);
            }
        }
        m_runLay->addStretch(1);

        if (!outSpec.fields.empty()) {
            m_tabs->addTab(m_outHost, T("输出"));
            QLabel* tip = new QLabel(T("本工具输出项（只读标签）："), m_outHost);
            tip->setObjectName(QLatin1String("hint"));
            tip->setWordWrap(true);
            m_outLay->addWidget(tip);
            m_outHolders.push_back(tip);
            for (size_t i = 0; i < outSpec.fields.size(); ++i) {
                const OutputField& f = outSpec.fields[i];
                QString line = qs(f.title.empty() ? f.key : f.title);
                if (!f.unit.empty()) {
                    line += QLatin1String(" (") + qs(f.unit) + QLatin1Char(')');
                }
                line += QLatin1String("  [") + qs(f.key) + QLatin1Char(']');
                QLabel* lab = new QLabel(line, m_outHost);
                lab->setWordWrap(true);
                m_outLay->addWidget(lab);
                m_outHolders.push_back(lab);
            }
            m_outLay->addStretch(1);
        }

        applyParamControlStyle(this);
    }

    // 当前用户能否改该字段
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

    // 示教写 param.learn，运行写 param.run
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        saveRows(m_runRows, &m_tool->params);
        saveRows(m_teachRows, &m_tool->param.learn);
    }

private:
    struct FieldRow {
        ParamField field;
        QWidget* editor;
        FieldRow() : editor(nullptr) {}
    };

    void saveRows(const std::vector<FieldRow>& rows, ParamMap* target)
    {
        if (!target) {
            return;
        }
        for (size_t i = 0; i < rows.size(); ++i) {
            const ParamField& f = rows[i].field;
            QWidget* w = rows[i].editor;
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
                    target->insert(f.key, box->isChecked());
                }
            } else if (f.type == ParamFieldType::Int) {
                QSpinBox* box = qobject_cast<QSpinBox*>(w);
                if (box) {
                    target->insert(f.key, box->value());
                }
            } else if (f.type == ParamFieldType::Double) {
                QDoubleSpinBox* box = qobject_cast<QDoubleSpinBox*>(w);
                if (box) {
                    target->insert(f.key, box->value());
                }
            } else {
                QLineEdit* line = qobject_cast<QLineEdit*>(w);
                QComboBox* box = qobject_cast<QComboBox*>(w);
                if (line) {
                    target->insert(f.key, ss(line->text()));
                } else if (box) {
                    target->insert(f.key, ss(box->currentText()));
                }
            }
        }
    }

    void clearPage(QVBoxLayout* lay, std::vector<FieldRow>* rows, std::vector<QWidget*>* holders)
    {
        if (rows) {
            rows->clear();
        }
        if (holders) {
            for (size_t i = 0; i < holders->size(); ++i) {
                if (lay) {
                    lay->removeWidget((*holders)[i]);
                }
                delete (*holders)[i];
                (*holders)[i] = nullptr;
            }
            holders->clear();
        }
        if (!lay) {
            return;
        }
        while (lay->count() > 0) {
            QLayoutItem* item = lay->takeAt(0);
            if (item) {
                delete item;
            } else {
                break;
            }
        }
    }

    void clearOutPage()
    {
        clearPage(m_outLay, 0, &m_outHolders);
    }

    ParamValue fieldValue(const ParamField& f, bool teach) const
    {
        const ParamMap& src = teach ? m_tool->param.learn : m_tool->params;
        if (src.contains(f.key)) {
            return src.value(f.key);
        }
        // 旧方案可能把示教键写在 run 里
        if (teach && m_tool->params.contains(f.key)) {
            return m_tool->params.value(f.key);
        }
        return f.def;
    }

    void addRow(const ParamField& f, bool teach, QVBoxLayout* lay,
                std::vector<FieldRow>* rows, std::vector<QWidget*>* holders)
    {
        FieldRow row;
        row.field = f;
        QWidget* editor = nullptr;
        const ParamValue cur = fieldValue(f, teach);
        if (f.type == ParamFieldType::Bool) {
            QCheckBox* box = mkParamCheck(this, QString());
            box->setChecked(cur.toBool(f.def.toBool()));
            connect(box, SIGNAL(stateChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else if (f.type == ParamFieldType::Int) {
            const int lo = f.hasMin ? static_cast<int>(f.minValue) : -1000000000;
            const int hi = f.hasMax ? static_cast<int>(f.maxValue) : 1000000000;
            QSpinBox* box = mkParamSpin(this, lo, hi);
            box->setValue(cur.toInt(f.def.toInt()));
            connect(box, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else if (f.type == ParamFieldType::Double) {
            const double lo = f.hasMin ? f.minValue : -1e12;
            const double hi = f.hasMax ? f.maxValue : 1e12;
            QDoubleSpinBox* box = mkParamDSpin(this, lo, hi, 4, 0.1);
            box->setValue(cur.toDouble(f.def.toDouble()));
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
            const QString curDev = qs(m_tool->deviceId);
            const int idx = box->findData(curDev);
            box->setCurrentIndex(idx >= 0 ? idx : 0);
            connect(box, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else if (!f.choices.empty()) {
            QComboBox* box = mkParamCombo(this);
            const std::string curS = cur.toString();
            const std::string def = f.def.toString();
            int pick = 0;
            for (size_t i = 0; i < f.choices.size(); ++i) {
                box->addItem(T(f.choices[i].c_str()));
                if (f.choices[i] == curS || (curS.empty() && f.choices[i] == def)) {
                    pick = static_cast<int>(i);
                }
            }
            box->setCurrentIndex(pick);
            connect(box, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
            editor = box;
        } else {
            QLineEdit* line = new QLineEdit(this);
            const std::string curS = cur.toString();
            line->setText(qs(curS.empty() ? f.def.toString() : curS));
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
                rows->push_back(row);
                QWidget* cell = mkParamCell(this, T(f.title.c_str()), wrap);
                lay->addWidget(cell);
                holders->push_back(cell);
                return;
            }
            editor = line;
        }
        row.editor = editor;
        if (editor) {
            editor->setEnabled(allowField(f.key));
        }
        rows->push_back(row);
        QWidget* cell = mkParamCell(this, T(f.title.c_str()), editor);
        lay->addWidget(cell);
        holders->push_back(cell);
    }

    QTabWidget* m_tabs;
    QWidget* m_runHost;
    QWidget* m_teachHost;
    QWidget* m_outHost;
    QVBoxLayout* m_runLay;
    QVBoxLayout* m_teachLay;
    QVBoxLayout* m_outLay;
    std::vector<FieldRow> m_runRows;
    std::vector<FieldRow> m_teachRows;
    std::vector<QWidget*> m_runHolders;
    std::vector<QWidget*> m_teachHolders;
    std::vector<QWidget*> m_outHolders;
};

// 按注册工具的参数说明生成参数页
ToolEditorBase* createGenericEditor(QWidget* parent)
{
    return new GenericEditor(parent);
}

}
