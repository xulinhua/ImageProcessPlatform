/**
 * @file UiPageHost.h
 * @brief 运行态自定义 UI 宿主
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UIPAGEHOST_H
#define VISION_UIPAGEHOST_H
#include "VisionUiExport.h"

#include "Solution.h"
#include "Station.h"
#include <QMap>
#include <QWidget>

class QLabel;
class QPushButton;

namespace HsCtrl {
class HsImageWindow;
}

namespace Vision {

/**
 * @class UiPageHost
 * @brief 按 UiPage 构建运行控件，并绑定全局变量 / 工站状态 / 控制信号
 */
class VISION_UI_API UiPageHost : public QWidget {
    Q_OBJECT
public:
    /** @brief 空宿主 */
    explicit UiPageHost(QWidget* parent = 0);

    /**
     * @brief 按页重建；sol 用于读全局变量与查工站实例
     * @param page 页描述
     * @param sol 方案（不拥有）
     * @param stations stationId → 运行实例（不拥有）
     */
    void bindPage(const UiPage& page, Solution* sol, const QMap<QString, Station*>& stations);

    /** @brief 刷新 Value/Label/OkNg/Image 绑定 */
    void refreshBindings();

signals:
    /** @brief 单次运行 */
    void runOnceRequested();
    /** @brief 循环运行 */
    void loopRequested();
    /** @brief 停止 */
    void stopRequested();
    /** @brief 清零 */
    void resetRequested();

private slots:
    /** @brief 按钮点击 → 按 action 发信号 */
    void onActionClicked();

private:
    /** @brief 解析 dataPath 前缀后的 id */
    static QString pathTail(const QString& path, const QString& prefix);

    Solution* m_sol;                          ///< 方案
    UiPage m_page;                            ///< 当前页
    QMap<QString, Station*> m_stations;       ///< 工站实例
    QMap<QString, QLabel*> m_labels;          ///< Value/Label/OkNg
    QMap<QString, HsCtrl::HsImageWindow*> m_images; ///< ImageView
    QWidget* m_canvas;                        ///< 画布
};

}

#endif
