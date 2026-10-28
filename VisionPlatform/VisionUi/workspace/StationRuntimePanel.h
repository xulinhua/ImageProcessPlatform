/**
 * @file StationRuntimePanel.h
 * @brief StationRuntimePanel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATIONRUNTIMEPANEL_H
#define VISION_STATIONRUNTIMEPANEL_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <HsCtrl/HsImageWindow.h>
#include <HsCommData/HsImage.h>
#include <HsCommData/Graphic.h>
#include <QImage>
#include <QVector>
#include <QWidget>

class QLabel;
class QHBoxLayout;
class QPushButton;
class QGridLayout;

namespace Vision {

class Station;

/**
 * @class StationRuntimePanel
 * @brief 单工站运行面板：图像网格、产量卡、结果历史（不含全局开始/停止）
 */
class VISION_UI_API StationRuntimePanel : public QWidget {
    Q_OBJECT
public:
    /** @brief 空面板 */
    explicit StationRuntimePanel(QWidget* parent = 0);

    /** @brief 绑定的工站配置 id；未绑定时为空 */
    ObjectId stationId() const;
    /** @brief 当前布局下第一个已使能的图像窗 */
    HsCtrl::HsImageWindow* imageWindow() const;
    /** @brief 绑定方案/工站配置/运行实例，按 runtimeView 刷新布局 */
    void bindContext(Solution* sol, StationCfg* st, Station* station);
    /**
     * @brief 仅更新运行实例指针（停止后置空、再开始时挂回），不刷布局、不清画面
     * @param station 运行实例；停止后传 0
     */
    void setRuntimeStation(Station* station);
    /** @brief 产量与节拍 */
    void setCounts(int total, int pass, int fail, int cycleMs);
    /** @brief 最近一次判定结果 */
    void setLastOk(bool ok);
    /** @brief 实时采集指示 */
    void setLive(bool live);
    /** @brief 追加一条历史（含缩略图） */
    void addResult(bool ok, const QString& line, const QImage& thumb);
    /** @brief 清空历史并复位计数 */
    void resetHistory();
    /** @brief 刷新本拍汇总图，再按各格配置刷缓存 */
    void setRunImages(const HsCommData::HsImage& source,
                      const HsCommData::HsImage& result,
                      const QVector<HsCommData::Graphic>& overlay);

private slots:
    /** @brief 历史过滤：全部、OK、NG */
    void onHistFilter();

private:
    /**
     * @class HistItem
     * @brief 一条结果历史
     */
    struct HistItem {
        bool ok;       ///< OK/NG
        QString tip;   ///< 摘要
        QString time;  ///< 时间
        QImage thumb;  ///< 缩略图
        HistItem() : ok(false) {}
    };
    /** @brief 按过滤重排历史条 */
    void rebuildHist();
    /** @brief 按当前行列与使能重建网格并显隐面板 */
    void applyLayout();
    /** @brief 把缓存帧画到各格 */
    void applyImages();
    /** @brief 工具实例名；空 id 为工站结果 */
    QString toolCaption(const ObjectId& id) const;

    HsCtrl::HsImageWindow* m_cells[9]; ///< 最多 9 个图像窗
    QLabel* m_cellTitles[9];            ///< 各格标题
    QWidget* m_cellPanes[9];            ///< 各格容器
    QGridLayout* m_gridLay;             ///< 图像网格
    QWidget* m_statsCard;               ///< 右侧统计卡
    QWidget* m_histBlock;               ///< 底部历史区
    QLabel* m_stationTitle;             ///< 工站名
    QLabel* m_banner;                   ///< OK/NG 横幅
    QLabel* m_total;                    ///< 总数
    QLabel* m_pass;                     ///< OK
    QLabel* m_fail;                     ///< NG
    QLabel* m_yield;                    ///< 良率
    QLabel* m_cycle;                    ///< 节拍
    QLabel* m_online;                   ///< 在线状态
    QLabel* m_live;                     ///< 实时指示
    QWidget* m_ngRow;                   ///< 历史条容器
    QHBoxLayout* m_ngLay;               ///< 历史条布局
    QPushButton* m_fAll;                ///< 过滤全部
    QPushButton* m_fOk;                 ///< 过滤 OK
    QPushButton* m_fNg;                 ///< 过滤 NG
    int m_histFilter;                   ///< 0全部 1OK 2NG
    Solution* m_sol;                    ///< 当前方案（不拥有）
    StationCfg* m_stationPtr;        ///< 当前工站配置（不拥有）
    Station* m_station;                 ///< 工站运行实例（不拥有）
    RuntimeView m_viewConfig;              ///< 当前画面配置副本
    HsCommData::HsImage m_lastSrc;     ///< 缓存源图
    HsCommData::HsImage m_lastRst;     ///< 缓存结果图
    QVector<HsCommData::Graphic> m_lastGfx;///< 缓存叠加
    QVector<HistItem> m_hist;           ///< 历史
    bool m_needFit;                   ///< 下次刷图时 fit（布局变化后）
};

}

#endif
