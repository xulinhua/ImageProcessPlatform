/**
 * @file RuntimeWorkspace.h
 * @brief RuntimeWorkspace头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUNTIMEWORKSPACE_H
#define VISION_RUNTIMEWORKSPACE_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <HsCtrl/HsImageWindow.h>
#include <HsCommData/HsImage.h>
#include <HsCommData/Graphic.h>
#include <QImage>
#include <QList>
#include <QMap>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QTabWidget;
class QGridLayout;
class QVBoxLayout;

namespace Vision {

class Station;
class StationRuntimePanel;

/**
 * @class RuntimeWorkspace
 * @brief 运行壳：全局开始/停止，按方案布局嵌套各工站面板（分页签或拼贴）
 */
class VISION_UI_API RuntimeWorkspace : public QWidget {
    Q_OBJECT
public:
    /** @brief 空运行页 */
    explicit RuntimeWorkspace(QWidget* parent = 0);

    /** @brief 当前焦点/首个工站面板的图像窗（调试入口） */
    HsCtrl::HsImageWindow* imageWindow() const;
    /** @brief 按方案重建各工站面板与壳布局；可选绑定设计态预览用的单站实例 */
    void bindContext(Solution* sol, StationCfg* previewSt, Station* previewStation);
    /** @brief 把运行实例挂到对应工站面板（已有面板时只换指针，不清画面） */
    void setStationInstance(const ObjectId& stationId, Station* station);
    /** @brief 全部面板卸下运行实例指针（停止后调用，保留画面与产量） */
    void detachAllStations();
    /** @brief 若宿主尚无面板则按方案建一次；已有则只更新方案指针与横幅 */
    void ensureHost(Solution* sol);
    /** @brief 按 id 取面板；无则 0 */
    StationRuntimePanel* panelFor(const ObjectId& stationId) const;
    /** @brief 全部面板 */
    QList<StationRuntimePanel*> panels() const;
    /** @brief 工站名（保留兼容；多站时各面板自带名） */
    void setStationName(const QString& name);
    /** @brief 方案名 */
    void setSolutionName(const QString& name);
    /** @brief 兼容旧调用：写入当前焦点面板计数 */
    void setCounts(int total, int pass, int fail, int cycleMs);
    /** @brief 兼容：焦点面板最近判定 */
    void setLastOk(bool ok);
    /** @brief 全部面板 Live 指示 */
    void setLive(bool live);
    /** @brief 兼容：焦点面板追加历史 */
    void addResult(bool ok, const QString& line, const QImage& thumb);
    /** @brief 清空全部工站历史与计数 */
    void resetHistory();
    /** @brief 兼容：焦点面板刷图 */
    void setRunImages(const HsCommData::HsImage& source,
                      const HsCommData::HsImage& result,
                      const QVector<HsCommData::Graphic>& overlay);
    /** @brief 按权限显隐开始、停止、清零、切方案 */
    void setControlPerms(bool canStartStop, bool canReset, bool canSwitch);
    /** @brief 画面配置按钮是否可点 */
    void setConfigureEnabled(bool on);
    /**
     * @brief 开始按钮随循环状态变色
     * @param running true=运行中绿色；false=未运行普通底
     */
    void setLoopRunning(bool running);
    /** @brief 语言热切换后刷新静态文案 */
    void retranslateUi();

signals:
    /** @brief 开始循环 */
    void startRequested();
    /** @brief 手动测试（等触发模式下跑一轮） */
    void softTriggerRequested();
    /** @brief 停止循环 */
    void stopRequested();
    /** @brief 清零产量 */
    void resetRequested();
    /** @brief 运行态切方案 */
    void switchSolutionRequested();
    /** @brief 打开画面配置 */
    void configureViewRequested();

private:
    /** @brief 按 Tabs/Tiles 重建各工站面板宿主 */
    void rebuildHost();
    /** @brief 当前焦点面板（页签当前页或第一块） */
    StationRuntimePanel* activePanel() const;

    Solution* m_sol;                              ///< 方案（不拥有）
    StationCfg* m_previewSt;                   ///< 设计预览工站配置
    Station* m_previewStation;                    ///< 设计预览运行实例
    QWidget* m_side;                              ///< 左侧开始停止列
    QWidget* m_host;                              ///< 右侧面板宿主
    QVBoxLayout* m_hostLay;                       ///< 宿主布局
    QTabWidget* m_tabs;                           ///< 分页签模式
    QWidget* m_tileHost;                          ///< 拼贴模式容器
    QGridLayout* m_tileLay;                       ///< 拼贴网格
    QPushButton* m_btnStart;                      ///< 开始
    QPushButton* m_btnTrigger;                    ///< 手动测试
    QPushButton* m_btnStop;                       ///< 停止
    QPushButton* m_btnReset;                      ///< 清零
    QPushButton* m_btnSwitch;                     ///< 切方案
    QPushButton* m_btnLayout;                     ///< 画面配置
    QLabel* m_solBanner;                          ///< 方案名
    QMap<QString, StationRuntimePanel*> m_panels; ///< stationId → 面板
};

}

#endif
