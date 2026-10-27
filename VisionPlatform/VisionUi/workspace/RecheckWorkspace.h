/**
 * @file RecheckWorkspace.h
 * @brief RecheckWorkspace头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RECHECKWORKSPACE_H
#define VISION_RECHECKWORKSPACE_H
#include "VisionUiExport.h"

#include "Solution.h"
#include "ToolBase.h"
#include <HsCtrl/HsImageWindow.h>
#include <HsCommData/HsImage.h>
#include <HsCommData/Graphic.h>
#include <QString>
#include <QVector>
#include <QWidget>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QProgressBar;
class QPushButton;

namespace Vision {

class Session;
class Station;
class Job;

/**
 * @class RecheckWorkspace
 * @brief 复检页：选文件夹与流程，批量跑图，表格浏览，点选看详情
 */
class VISION_UI_API RecheckWorkspace : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief 构造复检工作区
     * @param session 会话（不拥有）
     * @param solution 当前方案（不拥有）
     */
    RecheckWorkspace(Session* session, Solution* solution, QWidget* parent = 0);

    /** @brief 方案切换或工站列表变化后刷新下拉 */
    void reloadCombos();
    /** @brief 绑定工站运行实例（批跑前由主窗 ensure 后下发） */
    void setStation(Station* station);
    /** @brief 按权限启用开始/导出/报告/跳转 */
    void setRunPerms(bool canRun);
    /** @brief 退出软件：停止批跑标志（不等池；由主窗 drain） */
    void abortBatch();
    /** @brief 语言热切换后刷新静态文案 */
    void retranslateUi();

signals:
    /**
     * @brief 批跑前请求主窗绑定该工站运行实例
     * @param stationId 工站 id
     */
    void prepareStation(const QString& stationId);
    /**
     * @brief 跳转设计态并打开工具调试（可带复检图路径）
     * @param jobId 流程 id
     * @param toolId 工具 id（可空=流程首个工具）
     * @param imagePath 复检图全路径（可空）
     */
    void openDesignDebug(const QString& jobId, const QString& toolId, const QString& imagePath);

private slots:
    /** @brief 浏览图像文件夹 */
    void onBrowseFolder();
    /** @brief 工站切换时刷新流程下拉 */
    void onStationChanged();
    /** @brief 开始批量复检 */
    void onStart();
    /** @brief 停止批量复检 */
    void onStop();
    /** @brief 暂停 / 继续 */
    void onPauseResume();
    /** @brief 处理下一张图（投递到线程池） */
    void onStepNext();
    /** @brief 单张异步跑完（itemPtr 为堆上 Item*） */
    void onStepFinished(void* itemPtr);
    /** @brief 结果表选中变化，刷新详情 */
    void onRowSelected();
    /** @brief 过滤条件变化 */
    void onFilterChanged();
    /** @brief 导出 CSV */
    void onExportCsv();
    /** @brief 保存会话报告到 logs */
    void onSaveReport();
    /** @brief 用当前行跳转设计调试 */
    void onOpenDesignDebug();
    /** @brief 双击工具结果行跳转该工具调试 */
    void onToolDouble(int row, int col);

private:
    /** @brief 行判定：OK / NG / 失败（读图或注入失败） */
    enum Verdict {
        VerdictOk = 0,   ///< 检测通过
        VerdictNg,       ///< 流程跑完但判定 NG
        VerdictFail      ///< 读图/注入/实例失败，未正常出结果
    };

    /**
     * @class ToolRow
     * @brief 单工具结果摘要（详情表一行）
     */
    struct ToolRow {
        QString toolId;    ///< 工具 id
        QString name;      ///< 显示名
        bool ran;          ///< 本张是否跑过
        bool ok;           ///< OK/NG
        QString summary;   ///< 简要说明
        int elapsedMs;     ///< 工具耗时 ms；未跑为 -1
        ToolRow()
            : ran(false)
            , ok(false)
            , elapsedMs(-1)
        {}
    };

    /**
     * @class Item
     * @brief 单张复检结果（含详情缓存）
     */
    struct Item {
        QString path;                          ///< 图像全路径
        QString fileName;                      ///< 文件名
        Verdict verdict;                       ///< 判定
        bool ran;                              ///< 是否已处理
        int elapsedMs;                         ///< 耗时
        QString message;                       ///< 摘要
        HsCommData::HsImage source;           ///< 源图
        HsCommData::HsImage image;            ///< 结果图
        QVector<HsCommData::Graphic> graphics;    ///< 叠加
        QVector<ToolRow> tools;                ///< 各工具结果
        Item()
            : verdict(VerdictFail)
            , ran(false)
            , elapsedMs(0)
        {}
    };

    /** @brief 当前选中的工站配置 */
    StationCfg* currentStationConfig() const;
    /** @brief 当前选中的流程配置 */
    JobCfg* currentJobConfig() const;
    /** @brief 草稿流程：把采集工具 path 指到指定文件 */
    bool applyImageToDraft(JobCfg* draft, const std::string& imagePath, QString* err) const;
    /** @brief 从 Job 运行态收集各工具行 */
    void collectToolRows(Job* job, const JobCfg* formal, Item* item) const;
    /** @brief 刷新进度与统计标签 */
    void updateProgressLabels();
    /** @brief 更新开始/暂停/停止按钮使能 */
    void updateRunButtons();
    /** @brief 按过滤条件重填结果表（不丢 Item） */
    void refillTable();
    /** @brief 显示指定 Item 详情；空则清空 */
    void showDetail(const Item* item);
    /** @brief 表格行 → m_items 下标；无效为 -1 */
    int itemIndexFromRow(int row) const;
    /** @brief 当前选中 Item；无则 0 */
    const Item* currentItem() const;
    /** @brief 判定文案 */
    static QString verdictText(Verdict v);
    /** @brief 写报告到指定路径（JSON） */
    bool writeReportFile(const QString& path, QString* err) const;
    /** @brief 批跑结束时自动落一份报告 */
    void autoSaveReportIfNeeded();

    Session* m_session;                 ///< 会话
    Solution* m_sol;                    ///< 方案
    Station* m_station;                 ///< 工站运行实例，不拥有
    QLineEdit* m_folder;                ///< 图像目录
    QComboBox* m_stationCombo;          ///< 工站
    QComboBox* m_jobCombo;              ///< 流程
    QLabel* m_fileCount;                ///< 已扫描张数
    QPushButton* m_btnStart;            ///< 开始
    QPushButton* m_btnPause;            ///< 暂停/继续
    QPushButton* m_btnStop;             ///< 停止
    QProgressBar* m_progress;           ///< 进度
    QLabel* m_stats;                    ///< OK/NG/失败 统计
    QComboBox* m_filter;                ///< 过滤：全部/问题/NG/失败
    QPushButton* m_btnExport;           ///< 导出 CSV
    QPushButton* m_btnReport;           ///< 保存报告
    QPushButton* m_btnDebug;            ///< 打开设计调试
    QTableWidget* m_table;              ///< 结果表
    HsCtrl::HsImageWindow* m_view;     ///< 详情图像
    QLabel* m_detailTitle;              ///< 详情标题
    QTableWidget* m_toolTable;          ///< 工具结果表
    QVector<Item> m_items;              ///< 全部结果
    std::vector<std::string> m_queue;   ///< 待跑路径
    int m_queueIndex;                   ///< 下一张下标
    bool m_running;                     ///< 批跑中（含暂停）
    bool m_paused;                      ///< 已暂停
    bool m_stopRequested;               ///< 用户请求停止
    bool m_stepBusy;                    ///< 当前张已投递未完成
    bool m_canRun;                      ///< 运行权限
    QString m_sessionJobId;             ///< 本批流程 id（报告用）
    QString m_sessionStationId;         ///< 本批工站 id
    QString m_lastReportPath;           ///< 最近自动/手动报告路径
};

}

#endif
