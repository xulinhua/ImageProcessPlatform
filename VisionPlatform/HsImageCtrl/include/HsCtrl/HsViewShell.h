/**
 * @file HsViewShell.h
 * @brief 图像 / 点云外层壳。右键切换 2D·3D，不加浮动条
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCTRL_HSVIEWSHELL_H
#define HSCTRL_HSVIEWSHELL_H

#include <HsCtrl/HsImgCtrlExport.h>
#include <QWidget>

namespace HsCtrl {

class HsImageWindow;
class HsCloudWindow;

/** @brief 壳内显示模式 */
enum ViewMode {
    ViewModeImage2D = 0, ///< 图像窗
    ViewModeCloud3D      ///< 点云窗
};

/**
 * @class HsViewShell
 * @brief 叠放 HsImageWindow 与 HsCloudWindow；右键菜单切换模式
 */
class HSCTRL_EXPORT HsViewShell : public QWidget {
    Q_OBJECT
public:
    /** @brief 默认图像模式；点云窗懒创建 */
    explicit HsViewShell(QWidget* parent = 0);
    /** @brief 析构 */
    virtual ~HsViewShell();

    /** @brief 切换显示模式；进 3D 时懒建点云窗 */
    void setViewMode(ViewMode mode);
    /** @brief 当前模式 */
    ViewMode viewMode() const;

    /** @brief 无数据时右键「点云」灰显 */
    void setCloudModeEnabled(bool on);
    /** @brief 点云模式是否可选 */
    bool cloudModeEnabled() const;

    /** @brief 灌云后是否自动切到 3D（默认关） */
    void setAutoFollowResult(bool on);
    /** @brief 是否自动跟随 */
    bool autoFollowResult() const;

    /** @brief 图像窗（始终存在） */
    HsImageWindow* imageWindow() const;
    /** @brief 点云窗；尚未进过 3D 时可能为空，ensureCloudWindow 后非空 */
    HsCloudWindow* cloudWindow() const;
    /** @brief 确保点云窗已创建（不切换模式） */
    HsCloudWindow* ensureCloudWindow();

signals:
    /** @brief 模式变化 */
    void viewModeChanged(HsCtrl::ViewMode mode);

private slots:
    void onSwitchToCloud();
    void onSwitchToImage();
    void onCloudContextMenu(const QPoint& globalPos);
    void onCloudResetView();
    void onImageMenuExtras(QList<QAction*>* out);

private:
    void showCloudContextMenu(const QPoint& globalPos);
    void applyStackedVisibility();

    class Private;
    Private* d;
};

}

#endif
