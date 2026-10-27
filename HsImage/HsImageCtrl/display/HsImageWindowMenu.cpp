/**
 * @file HsImageWindowMenu.cpp
 * @brief HsImageWindowMenu实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "HsImageWindowMenu.h"
#include <HsCtrl/HsImageWindow.h>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
namespace HsCtrl {
// 测量子菜单里的工具名
static QString measureToolTitle(DrawTool tool)
{
    switch (tool) {
    case ToolBrowse:
        return QString::fromUtf8("浏览");
    case ToolMeasureDist:
        return QString::fromUtf8("距离");
    case ToolMeasureAngle:
        return QString::fromUtf8("夹角");
    case ToolMeasureRadius:
        return QString::fromUtf8("半径");
    case ToolMeasureRect:
        return QString::fromUtf8("矩形");
    case ToolMeasureEllipse:
        return QString::fromUtf8("椭圆");
    default:
        return QString();
    }
}

// 往测量菜单加一项，并标出当前工具
static QAction* addMeasureTool(QMenu* menu, QActionGroup* group, DrawTool tool, DrawTool current)
{
    QAction* act = menu->addAction(measureToolTitle(tool));
    act->setCheckable(true);
    act->setData(static_cast<int>(tool));
    act->setChecked(current == tool);
    group->addAction(act);
    return act;
}

// 弹出图像窗口右键菜单
void HsImageWindowMenu::popup(HsImageWindow* window, const QPoint& globalPos,
                              bool hasImage, bool viewReady, bool canCancelDraw,
                              const QString& measureId)
{
    if (!window) {
        return;
    }
    QMenu menu(window);
    QAction* snapAct = menu.addAction(QString::fromUtf8("保存快照..."));
    QAction* origAct = menu.addAction(QString::fromUtf8("导出原图..."));
    origAct->setEnabled(hasImage);

    QMenu* viewMenu = menu.addMenu(QString::fromUtf8("视图"));
    QAction* zoomInAct = viewMenu->addAction(QString::fromUtf8("放大"));
    QAction* zoomOutAct = viewMenu->addAction(QString::fromUtf8("缩小"));
    QAction* fitAct = viewMenu->addAction(QString::fromUtf8("适应窗口"));
    QAction* resetAct = viewMenu->addAction(QString::fromUtf8("实际像素"));
    zoomInAct->setEnabled(viewReady);
    zoomOutAct->setEnabled(viewReady);
    fitAct->setEnabled(viewReady);
    resetAct->setEnabled(viewReady);

    QAction* clearMeasAct = nullptr;
    QActionGroup* measureGroup = nullptr;
    const std::vector<DrawTool> measureShown = window->measureTools();
    if (!measureShown.empty()) {
        QMenu* measureMenu = menu.addMenu(QString::fromUtf8("测量"));
        measureGroup = new QActionGroup(measureMenu);
        measureGroup->setExclusive(true);
        const DrawTool current = window->drawTool();
        addMeasureTool(measureMenu, measureGroup, ToolBrowse, current);
        for (size_t i = 0; i < measureShown.size(); ++i) {
            const QString title = measureToolTitle(measureShown[i]);
            if (title.isEmpty()) {
                continue;
            }
            addMeasureTool(measureMenu, measureGroup, measureShown[i], current);
        }
        measureMenu->addSeparator();
        clearMeasAct = measureMenu->addAction(QString::fromUtf8("清除测量"));
        clearMeasAct->setEnabled(!window->measureResultLines().isEmpty());
    }

    QMenu* showMenu = menu.addMenu(QString::fromUtf8("显示"));
    QAction* imageAct = showMenu->addAction(QString::fromUtf8("底图"));
    QAction* stackAct = showMenu->addAction(QString::fromUtf8("叠层"));
    QAction* drawAct = nullptr;
    if (window->windowMode() != WindowRuntime) {
        drawAct = showMenu->addAction(QString::fromUtf8("绘制"));
        drawAct->setCheckable(true);
        drawAct->setChecked(window->isDrawingVisible());
    }
    QAction* inspectAct = showMenu->addAction(QString::fromUtf8("像素检验"));
    QAction* hudAct = showMenu->addAction(QString::fromUtf8("状态栏"));
    imageAct->setCheckable(true);
    stackAct->setCheckable(true);
    inspectAct->setCheckable(true);
    hudAct->setCheckable(true);
    imageAct->setChecked(window->isImageVisible());
    stackAct->setChecked(window->isStackVisible());
    inspectAct->setChecked(window->isPixelInspect());
    hudAct->setChecked(window->isHudVisible());

    QMenu* barMenu = menu.addMenu(QString::fromUtf8("工具栏"));
    QAction* barVis = barMenu->addAction(QString::fromUtf8("显示工具栏"));
    barVis->setCheckable(true);
    barVis->setChecked(window->isOverlayToolbarVisible());
    QAction* measVis = nullptr;
    if (!window->measureTools().empty()) {
        measVis = barMenu->addAction(QString::fromUtf8("显示测量"));
        measVis->setCheckable(true);
        measVis->setChecked(window->isMeasureToolbarVisible());
    }
    QMenu* dockMenu = barMenu->addMenu(QString::fromUtf8("位置"));
    QActionGroup* dockGroup = new QActionGroup(dockMenu);
    dockGroup->setExclusive(true);
    const Qt::Corner corner = window->overlayToolbarCorner();
    QAction* dockTl = dockMenu->addAction(QString::fromUtf8("左上"));
    QAction* dockTr = dockMenu->addAction(QString::fromUtf8("右上"));
    QAction* dockBl = dockMenu->addAction(QString::fromUtf8("左下"));
    QAction* dockBr = dockMenu->addAction(QString::fromUtf8("右下"));
    dockTl->setCheckable(true);
    dockTr->setCheckable(true);
    dockBl->setCheckable(true);
    dockBr->setCheckable(true);
    dockTl->setChecked(corner == Qt::TopLeftCorner);
    dockTr->setChecked(corner == Qt::TopRightCorner);
    dockBl->setChecked(corner == Qt::BottomLeftCorner);
    dockBr->setChecked(corner == Qt::BottomRightCorner);
    dockTl->setData(static_cast<int>(Qt::TopLeftCorner));
    dockTr->setData(static_cast<int>(Qt::TopRightCorner));
    dockBl->setData(static_cast<int>(Qt::BottomLeftCorner));
    dockBr->setData(static_cast<int>(Qt::BottomRightCorner));
    dockGroup->addAction(dockTl);
    dockGroup->addAction(dockTr);
    dockGroup->addAction(dockBl);
    dockGroup->addAction(dockBr);
    QMenu* orientMenu = barMenu->addMenu(QString::fromUtf8("排列"));
    QActionGroup* orientGroup = new QActionGroup(orientMenu);
    orientGroup->setExclusive(true);
    const Qt::Orientation orient = window->overlayToolbarOrientation();
    QAction* orientV = orientMenu->addAction(QString::fromUtf8("垂直"));
    QAction* orientH = orientMenu->addAction(QString::fromUtf8("水平"));
    orientV->setCheckable(true);
    orientH->setCheckable(true);
    orientV->setChecked(orient == Qt::Vertical);
    orientH->setChecked(orient == Qt::Horizontal);
    orientV->setData(static_cast<int>(Qt::Vertical));
    orientH->setData(static_cast<int>(Qt::Horizontal));
    orientGroup->addAction(orientV);
    orientGroup->addAction(orientH);

    menu.addSeparator();
    QAction* delAct = nullptr;
    if (!measureId.isEmpty()) {
        delAct = menu.addAction(QString::fromUtf8("删除此项"));
    }
    QAction* clearAct = menu.addAction(QString::fromUtf8("清除显示"));
    QAction* cancelAct = nullptr;
    if (canCancelDraw) {
        menu.addSeparator();
        cancelAct = menu.addAction(QString::fromUtf8("取消绘制"));
    }

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) {
        return;
    }
    if (chosen == snapAct) {
        window->promptSaveSnapshot();
    } else if (chosen == origAct) {
        window->promptSaveOriginal();
    } else if (chosen == zoomInAct) {
        window->zoomIn();
    } else if (chosen == zoomOutAct) {
        window->zoomOut();
    } else if (chosen == fitAct) {
        window->fitToWindow();
    } else if (chosen == resetAct) {
        window->resetView();
    } else if (measureGroup && chosen->actionGroup() == measureGroup) {
        window->setDrawTool(static_cast<DrawTool>(chosen->data().toInt()));
    } else if (clearMeasAct && chosen == clearMeasAct) {
        window->clearMeasures();
    } else if (chosen == imageAct) {
        window->setImageVisible(imageAct->isChecked());
    } else if (chosen == stackAct) {
        window->setStackVisible(stackAct->isChecked());
    } else if (drawAct && chosen == drawAct) {
        window->setDrawingVisible(drawAct->isChecked());
    } else if (chosen == inspectAct) {
        window->setPixelInspect(inspectAct->isChecked());
    } else if (chosen == hudAct) {
        window->setHudVisible(hudAct->isChecked());
    } else if (chosen == barVis) {
        window->setOverlayToolbarVisible(barVis->isChecked());
    } else if (measVis && chosen == measVis) {
        window->setMeasureToolbarVisible(measVis->isChecked());
    } else if (chosen->actionGroup() == dockGroup) {
        window->setOverlayToolbarCorner(static_cast<Qt::Corner>(chosen->data().toInt()));
    } else if (chosen->actionGroup() == orientGroup) {
        window->setOverlayToolbarOrientation(static_cast<Qt::Orientation>(chosen->data().toInt()));
    } else if (delAct && chosen == delAct) {
        window->removeMeasure(measureId);
    } else if (chosen == clearAct) {
        window->clearDisplay();
    } else if (cancelAct && chosen == cancelAct) {
        window->cancelActiveDraw();
    }
}
}
