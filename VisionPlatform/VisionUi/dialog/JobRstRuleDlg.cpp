/**
 * @file JobRstRuleDlg.cpp
 * @brief JobRstRuleDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "JobRstRuleDlg.h"
#include "JobRstRulePanel.h"
#include "QtConv.h"
#include "Theme.h"
#include "UiText.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace Vision {

// 输出结果配置独立对话框
JobRstRuleDlg::JobRstRuleDlg(JobCfg* job, bool editable, QWidget* parent)
    : QDialog(parent)
    , m_panel(nullptr)
    , m_job(job)
{
    QString title = T("输出结果配置");
    if (job && !job->name.empty()) {
        title = qs(job->name) + QLatin1Char(' ') + QString(QChar(0x00B7)) + QLatin1Char(' ') + title;
    }
    setWindowTitle(title);
    resize(960, 640);
    setMinimumSize(720, 480);
    enableDialogResize(this);

    m_panel = new JobRstRulePanel(this);
    m_panel->setJob(job);
    m_panel->setEditable(editable);
    if (job) {
        m_panel->loadFromJob(job);
    }

    QDialogButtonBox* box = new QDialogButtonBox(
        editable ? (QDialogButtonBox::Ok | QDialogButtonBox::Cancel) : QDialogButtonBox::Close,
        this);
    if (editable) {
        connect(box, SIGNAL(accepted()), this, SLOT(accept()));
        connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    } else {
        connect(box, SIGNAL(accepted()), this, SLOT(reject()));
        connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    }

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, title);
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(m_panel, 1);
    lay->addWidget(box);
}

// 确定：写回 JobCfg
void JobRstRuleDlg::accept()
{
    if (m_panel && m_job) {
        m_panel->flushToJob(m_job);
    }
    QDialog::accept();
}

}

#include "moc_JobRstRuleDlg.cpp"
