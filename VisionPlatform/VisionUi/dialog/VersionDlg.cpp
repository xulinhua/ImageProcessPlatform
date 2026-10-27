/**
 * @file VersionDlg.cpp
 * @brief VersionDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VersionDlg.h"
#include "QtConv.h"
#include "Theme.h"
#include "ToolBase.h"
#include "ToolEnums.h"
#include "UiText.h"
#include "VisionVersion.h"
#include "AlgLibKind.h"
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Vision {

namespace {

// 填一行两列表
void setRow2(QTableWidget* table, int row, const QString& a, const QString& b)
{
    table->setItem(row, 0, new QTableWidgetItem(a));
    table->setItem(row, 1, new QTableWidgetItem(b));
}

// 填一行四列表

// 填一行十列（末四列：Hal / Hik / Hray / CV）
void setRow10(QTableWidget* table, int row,
              const QString& a, const QString& b, const QString& c, const QString& d,
              const QString& e, const QString& f, const QString& g, const QString& h,
              const QString& i, const QString& j)
{
    table->setItem(row, 0, new QTableWidgetItem(a));
    table->setItem(row, 1, new QTableWidgetItem(b));
    table->setItem(row, 2, new QTableWidgetItem(c));
    table->setItem(row, 3, new QTableWidgetItem(d));
    table->setItem(row, 4, new QTableWidgetItem(e));
    table->setItem(row, 5, new QTableWidgetItem(f));
    table->setItem(row, 6, new QTableWidgetItem(g));
    table->setItem(row, 7, new QTableWidgetItem(h));
    table->setItem(row, 8, new QTableWidgetItem(i));
    table->setItem(row, 9, new QTableWidgetItem(j));
}
void setRow4(QTableWidget* table, int row, const QString& a, const QString& b,
             const QString& c, const QString& d)
{
    table->setItem(row, 0, new QTableWidgetItem(a));
    table->setItem(row, 1, new QTableWidgetItem(b));
    table->setItem(row, 2, new QTableWidgetItem(c));
    table->setItem(row, 3, new QTableWidgetItem(d));
}

} // namespace

// 关于对话框：软件摘要 + 模块表 + 工具表
VersionDlg::VersionDlg(QWidget* parent)
    : QDialog(parent)
    , m_summary(nullptr)
    , m_modules(nullptr)
    , m_tools(nullptr)
{
    setWindowTitle(T("版本管理"));
    setModal(true);
    resize(1280, 560);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setObjectName(QLatin1String("hint"));

    m_modules = new QTableWidget(this);
    m_modules->setColumnCount(2);
    m_modules->setHorizontalHeaderLabels(QStringList() << T("模块") << T("版本"));
    m_modules->horizontalHeader()->setStretchLastSection(true);
    m_modules->verticalHeader()->setVisible(false);
    m_modules->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modules->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modules->setAlternatingRowColors(true);

    m_tools = new QTableWidget(this);
    m_tools->setColumnCount(11);
    m_tools->setHorizontalHeaderLabels(QStringList()
                                       << T("注册名") << T("显示名") << T("版本") << T("工具修改时间")
                                       << T("大类") << T("算法工具")
                                       << QString::fromLatin1(algLibKindTitle(AlgLibHal))
                                       << QString::fromLatin1(algLibKindTitle(AlgLibHik))
                                       << QString::fromLatin1(algLibKindTitle(AlgLibHray))
                                       << QString::fromLatin1(algLibKindTitle(AlgLibCv)) << QString());
    m_tools->horizontalHeader()->setStretchLastSection(false);
    m_tools->horizontalHeader()->setSectionResizeMode(10, QHeaderView::Stretch);
    m_tools->verticalHeader()->setVisible(false);
    m_tools->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tools->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tools->setAlternatingRowColors(true);

    QLabel* modTitle = new QLabel(T("模块"), this);
    QLabel* toolTitle = new QLabel(T("已注册工具"), this);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    box->button(QDialogButtonBox::Ok)->setText(T("关闭"));
    setButtonRole(box->button(QDialogButtonBox::Ok), "primary");
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(m_summary);
    lay->addWidget(modTitle);
    lay->addWidget(m_modules, 1);
    lay->addWidget(toolTitle);
    lay->addWidget(m_tools, 2);
    lay->addWidget(box);

    fillTables();
}

// 填充摘要与两张表
void VersionDlg::fillTables()
{
    const QString summary = QString::fromUtf8(appProductName())
                            + QLatin1Char('\n')
                            + T("软件版本：") + QString::fromLatin1(appVersion())
                            + QLatin1Char('\n')
                            + T("构建时间：") + qs(buildStamp())
                            + QLatin1Char('\n')
                            + T("工具链：VS2015 (v140) + Qt 5.9.9 + C++11");
    m_summary->setText(summary);

    const std::vector<VersionInfo> mods = moduleVersions();
    m_modules->setRowCount(static_cast<int>(mods.size()));
    for (int i = 0; i < static_cast<int>(mods.size()); ++i) {
        const VersionInfo& v = mods[static_cast<size_t>(i)];
        setRow2(m_modules, i, qs(v.name), qs(v.version));
    }
    m_modules->resizeColumnsToContents();

    const std::vector<VersionInfo> tools = toolVersions();
    m_tools->setRowCount(static_cast<int>(tools.size()));
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const VersionInfo& v = tools[static_cast<size_t>(i)];
        QString typeTitle = T("-");
        ToolBase* tb = ToolRegistry::instance().find(v.id);
        if (tb) {
            typeTitle = QString::fromUtf8(toolTypeTitle(tb->type()).c_str());
        }
        QString algo = v.algoTool ? T("是") : T("否");
        QString mtime = v.modified.empty() ? QString(QLatin1String("-")) : qs(v.modified);
        const QString yes = T("是");
        const QString no = QString(QLatin1String("-"));
        setRow10(m_tools, i, qs(v.id), qs(v.name), qs(v.version), mtime, typeTitle, algo,
                 v.hasAlgLib[AlgLibHal] ? yes : no,
                 v.hasAlgLib[AlgLibHik] ? yes : no,
                 v.hasAlgLib[AlgLibHray] ? yes : no,
                 v.hasAlgLib[AlgLibCv] ? yes : no);
    }
    // 数据列按文字宽度，多出的宽度留给最后一列空列
    for (int c = 0; c < 10; ++c) {
        m_tools->resizeColumnToContents(c);
    }
}

}

#include "moc_VersionDlg.cpp"