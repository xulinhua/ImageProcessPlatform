/**
 * @file StationMgrDlg.h
 * @brief StationMgrDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_STATIONMANAGERDIALOG_H
#define VISION_STATIONMANAGERDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>

class QListWidget;
class QLabel;
class QButtonGroup;
class QTableWidget;
class QHBoxLayout;

namespace Vision {

class Session;
struct Solution;

/**
 * @class StationMgrDlg
 * @brief 工站管理：列表、概要、绑定设备摘要、通信通道、JobCfg 摘要
 */
class VISION_UI_API StationMgrDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造工站管理对话框
     * @param solution 当前方案（不拥有）
     */
    StationMgrDlg(Session* session, Solution* solution, QWidget* parent = 0);

signals:
    /**
     * @brief 请求打开工站配置
     * @param stationId 工站 id
     */
    void openStationRequested(const QString& stationId);
    /**
     * @brief 请求打开 JobCfg 配置
     * @param jobId 流程 id
     */
    void openJobRequested(const QString& jobId);

private slots:
    /** @brief 添加空工站 */
    void onAdd();
    /** @brief 删除当前工站（级联其 JobCfg） */
    void onRemove();
    /** @brief 应用通信选择并发出 openStationRequested */
    void onOpen();
    /** @brief 打开选中（或首个）JobCfg */
    void onOpenJob();
    /** @brief 列表选中变化，刷新右侧详情 */
    void onSel();
    /** @brief 把通信通道写回当前工站 */
    void onApply();
    /**
     * @brief 双击 JobCfg 表打开该流程
     * @param row 行号
     * @param col 列号（未使用）
     */
    void onJobDouble(int row, int col);

private:
    /** @brief 重填工站列表并尽量保持选中 */
    void refresh();
    /** @brief 按当前工站填充概要、设备芯片、通信、JobCfg 表 */
    void fillDetail();
    /** @brief 把当前通信选择写入 StationCfg::commChannelId */
    void applyComm();
    /**
     * @brief 选中对应通信按钮；找不到则回落到第一项
     * @param id 通道 id，空表示「无」
     */
    void selectComm(const QString& id);
    /**
     * @brief 当前选中工站 id
     * @return 无选中时为空
     */
    QString currentId() const;
    /**
     * @brief 当前勾选的通信通道 id
     * @return 「无」时为空
     */
    QString currentCommId() const;

    Session* m_session;         ///< 会话（不拥有）
    Solution* m_sol;           ///< 当前方案（不拥有）
    QListWidget* m_list;      ///< 工站列表
    QLabel* m_name;            ///< 名称
    QLabel* m_code;            ///< 工位编号
    QLabel* m_status;          ///< 状态
    QLabel* m_jobCount;       ///< JobCfg 数量
    QLabel* m_devCount;        ///< 绑定设备数
    QLabel* m_arrange;        ///< 流程执行方式
    QHBoxLayout* m_chips;      ///< 绑定设备类型芯片
    QButtonGroup* m_commGroup; ///< 通信通道互斥组
    QHBoxLayout* m_commLay;    ///< 通信按钮行
    QTableWidget* m_jobs;      ///< 本工站 JobCfg 摘要
    /** @brief 按方案通信台账重建通道按钮 */
    void rebuildCommChoices();
};

}

#endif
