/**
 * @file ProcessCombo.cpp
 * @brief 共享进程画面时选进程的下拉框
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ProcessCombo.h"
#include "WindowWatch.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QLineEdit>

namespace Vision {

ProcessCombo::ProcessCombo(QWidget* parent)
    : QComboBox(parent)
{
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setMaxVisibleItems(16);
    lineEdit()->setPlaceholderText(QString::fromUtf8("选择或输入进程名，如 VisionPlatform.exe"));
    reload();
}

QString ProcessCombo::process() const
{
    return currentText().trimmed();
}

void ProcessCombo::setProcess(const QString& name)
{
    setEditText(name);
}

void ProcessCombo::reload()
{
    const QString text = currentText();
    const std::vector<WindowWatch::Process> list = WindowWatch::processes();
    QFileIconProvider icons;
    clear();
    for (size_t i = 0; i < list.size(); ++i) {
        const QString path = QString::fromStdWString(list[i].path);
        addItem(icons.icon(QFileInfo(path)), QString::fromStdWString(list[i].name));
        setItemData(count() - 1, path, Qt::ToolTipRole);
    }
    setEditText(text);
}

void ProcessCombo::showPopup()
{
    reload();
    QComboBox::showPopup();
}

}
