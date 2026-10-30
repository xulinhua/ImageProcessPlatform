/**
 * @file HsImageWindowMenu.h
 * @brief HsImageWindowMenu头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_HSIMAGEWINDOWMENU_H
#define HSIMAGE_HSIMAGEWINDOWMENU_H
#include <QList>
#include <QPoint>
#include <QString>
class QAction;
class QWidget;
namespace HsCtrl {
class HsImageWindow;
/** @class HsImageWindowMenu @brief 图像窗口右键菜单 */
class HsImageWindowMenu {
public:
    /**
     * @brief 弹出菜单并执行选中项
     * @param hasImage 是否有底图
     * @param viewReady 视口是否可缩放
     * @param canCancelDraw 是否有未提交的橡皮筋
     * @param measureId 右键命中的测量 id，点在空白则为空
     * @param trailing 追加在菜单末尾的动作（如壳的「切换到点云」）；可空
     */
    static void popup(HsImageWindow* window, const QPoint& globalPos,
                      bool hasImage, bool viewReady, bool canCancelDraw,
                      const QString& measureId,
                      const QList<QAction*>& trailing = QList<QAction*>());
};
}
#endif
