/**
 * @file Acquire3DEditor.cpp
 * @brief 三维采集参数页：来源 + 路径，浏览随来源切换文件/文件夹
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditors.h"
#include "ToolEditorBase.h"
#include "ToolParamUi.h"
#include "QtConv.h"
#include "UiText.h"
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

/**
 * @class Acquire3DEditor
 * @brief 对齐二维文件采集：文件夹循环 / 单个点云 + 浏览
 */
class Acquire3DEditor : public ToolEditorBase {
public:
    explicit Acquire3DEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_mode = mkParamCombo(this);
        m_mode->addItem(T("文件夹循环"), QLatin1String("folder"));
        m_mode->addItem(T("单个点云"), QLatin1String("file"));
        m_path = new QLineEdit(this);
        m_path->setObjectName(QLatin1String("toolDebugParamLine"));
        QPushButton* browse = new QPushButton(T("浏览…"), this);
        browse->setFixedHeight(28);
        connect(browse, &QPushButton::clicked, this, &Acquire3DEditor::onBrowse);
        QWidget* pathWrap = new QWidget(this);
        QHBoxLayout* pathLay = new QHBoxLayout(pathWrap);
        pathLay->setContentsMargins(0, 0, 0, 0);
        pathLay->setSpacing(8);
        pathLay->addWidget(m_path, 1);
        pathLay->addWidget(browse);
        pathWrap->setFixedHeight(28);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamCell(this, T("来源"), m_mode));
        addParamStackItem(lay, mkParamCell(this, T("路径"), pathWrap));
        connect(m_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_path, SIGNAL(editingFinished()), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        const QString mode = qs(m_tool->params.value("sourceMode").toString());
        const int ix = m_mode->findData(mode.isEmpty() ? QLatin1String("folder") : mode);
        m_mode->setCurrentIndex(ix >= 0 ? ix : 0);
        m_path->setText(qs(m_tool->params.value("path").toString()));
    }

    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        const std::string oldPath = m_tool->params.value("path").toString();
        const std::string oldMode = m_tool->params.value("sourceMode").toString();
        const std::string newPath = ss(m_path->text().trimmed());
        const std::string newMode = ss(m_mode->currentData().toString());
        m_tool->params.insert("path", newPath);
        m_tool->params.insert("sourceMode", newMode);
        if (newPath != oldPath || newMode != oldMode) {
            m_tool->params.insert("index", 0);
            m_tool->params.raw().erase("grabPath");
            m_tool->params.insert("pathChanged", 1);
        }
    }

private:
    void onBrowse()
    {
        QString p;
        const bool single = m_mode->currentData().toString() == QLatin1String("file");
        if (single) {
            p = QFileDialog::getOpenFileName(this, T("单个点云"), QString(),
                                             T("点云 (*.ply *.pcd);;所有文件 (*.*)"));
        } else {
            p = QFileDialog::getExistingDirectory(this, T("点云文件夹"));
        }
        if (!p.isEmpty()) {
            m_path->setText(p);
            onEdited();
        }
    }

    QComboBox* m_mode;
    QLineEdit* m_path;
};

ToolEditorBase* createAcquire3DEditor(QWidget* parent)
{
    return new Acquire3DEditor(parent);
}

}
