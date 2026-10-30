/**
 * @file ToolLibrary.cpp
 * @brief ToolLibrary实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolLibrary.h"
#include "ToolBase.h"
#include "CompositeMgr.h"
#include "ToolTreeIcons.h"
#include "IconAssets.h"
#include "QtConv.h"
#include "UiText.h"
#include <QColor>
#include <QSize>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <vector>

namespace Vision {

namespace {

// 分类浅底色（与工具图标色系对应）
QColor categoryBadgeColor(ToolType ty)
{
    switch (ty) {
    case ToolType::Acquire: return QColor(0xE3, 0xF2, 0xFD);
    case ToolType::Preprocess: return QColor(0xE8, 0xF5, 0xE9);
    case ToolType::Locate: return QColor(0xF3, 0xE5, 0xF5);
    case ToolType::Detect: return QColor(0xFF, 0xEB, 0xEE);
    case ToolType::Logic: return QColor(0xE8, 0xEA, 0xF6);
    case ToolType::Comm: return QColor(0xE0, 0xF2, 0xF1);
    case ToolType::RstProcess: return QColor(0xFF, 0xF3, 0xE0);
    case ToolType::Calib: return QColor(0xE1, 0xF5, 0xFE);
    case ToolType::Acquire3D: return QColor(0xE0, 0xF7, 0xFA);
    case ToolType::Prepare3D: return QColor(0xE8, 0xF5, 0xE9);
    case ToolType::Locate3D: return QColor(0xF3, 0xE5, 0xF5);
    case ToolType::Measure3D: return QColor(0xFF, 0xF8, 0xE1);
    case ToolType::Detect3D: return QColor(0xFF, 0xEB, 0xEE);
    case ToolType::Calib3D: return QColor(0xE1, 0xF5, 0xFE);
    case ToolType::Infer: return QColor(0xFC, 0xE4, 0xEC);
    case ToolType::Identify: return QColor(0xE0, 0xF2, 0xF1);
    }
    return QColor(0xEE, 0xEE, 0xEE);
}

} // namespace

// 按 ToolType 填父节点，再把每个已注册具体工具挂到对应类型下
void fillRegisteredToolTree(QTreeWidget* lib, bool withFlowStructure)
{
    if (!lib) {
        return;
    }
    lib->clear();
    lib->setIconSize(QSize(20, 20));
    const int nType = toolTypeCount();
    const int nShow = toolTypeDisplayCount();
    std::vector<QTreeWidgetItem*> cats;
    cats.resize(static_cast<size_t>(nType), 0);
    for (int i = 0; i < nShow; ++i) {
        const ToolType ty = toolTypeDisplayAt(i);
        const int idx = toolTypeToInt(ty);
        if (idx < 0 || idx >= nType) {
            continue;
        }
        QTreeWidgetItem* cat = new QTreeWidgetItem(
            lib, QStringList(QString::fromUtf8(toolTypeTitle(ty).c_str())));
        const int key = toolTreeIconKeyForType(ty);
        const QIcon raw = toolTreeIcon(key);
        cat->setIcon(0, badgeVisionIcon(raw, categoryBadgeColor(ty), 20));
        cats[static_cast<size_t>(idx)] = cat;
    }

    // 具体工具：UserRole 存 toolName，提示里带版本和描述
    const std::vector<ToolBase*> tools = ToolRegistry::instance().all();
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        const ToolDesc d = tools[static_cast<size_t>(i)]->desc();
        const int ty = toolTypeToInt(d.type);
        if (ty < 0 || ty >= nType || !cats[static_cast<size_t>(ty)]) {
            continue;
        }
        QTreeWidgetItem* it = new QTreeWidgetItem(cats[static_cast<size_t>(ty)], QStringList(qs(d.displayName)));
        it->setData(0, Qt::UserRole, qs(d.toolName));
        it->setIcon(0, toolTreeIconForName(qs(d.toolName), toolTreeIconKeyForType(d.type)));
        QString tip = qs(d.toolName) + QLatin1String("  v") + qs(d.version);
        if (!d.description.empty()) {
            tip += QLatin1Char('\n') + qs(d.description);
        }
        it->setToolTip(0, tip);
    }
    // 流程结构节点，UserRole 为 group.parallel / group.if / group.loop
    if (withFlowStructure) {
        QTreeWidgetItem* st = new QTreeWidgetItem(lib, QStringList(T("流程结构")));
        st->setIcon(0, badgeVisionIcon(toolTreeIcon(ToolTreeIconParallel), QColor(0xE3, 0xF2, 0xFD), 20));
        QTreeWidgetItem* pg = new QTreeWidgetItem(st, QStringList(T("并行组")));
        pg->setData(0, Qt::UserRole, QLatin1String("group.parallel"));
        pg->setIcon(0, toolTreeIcon(ToolTreeIconParallel));
        QTreeWidgetItem* ig = new QTreeWidgetItem(st, QStringList(T("条件组 If/Else")));
        ig->setData(0, Qt::UserRole, QLatin1String("group.if"));
        ig->setIcon(0, toolTreeIcon(ToolTreeIconIf));
        QTreeWidgetItem* lg = new QTreeWidgetItem(st, QStringList(T("循环组")));
        lg->setData(0, Qt::UserRole, QLatin1String("group.loop"));
        lg->setIcon(0, toolTreeIcon(ToolTreeIconBranch));
    }
    // 组合工具按作用域分两类，UserRole 为 composite:作用域:id
    QTreeWidgetItem* libCat = new QTreeWidgetItem(lib, QStringList(QString::fromUtf8("独立组合工具")));
    QTreeWidgetItem* solCat = new QTreeWidgetItem(lib, QStringList(QString::fromUtf8("项目组合工具")));
    const std::vector<CompositeDef*> defs = CompositeMgr::instance().all();
    for (size_t i = 0; i < defs.size(); ++i) {
        if (!defs[i]) {
            continue;
        }
        QTreeWidgetItem* parent = defs[i]->scope == CompositeLibrary ? libCat : solCat;
        QTreeWidgetItem* it = new QTreeWidgetItem(parent, QStringList(qs(defs[i]->name)));
        const QString key = QLatin1String("composite:")
            + QString::number(static_cast<int>(defs[i]->scope))
            + QLatin1Char(':')
            + qs(defs[i]->id.toString());
        it->setData(0, Qt::UserRole, key);
    }
    lib->expandAll();
}

}
