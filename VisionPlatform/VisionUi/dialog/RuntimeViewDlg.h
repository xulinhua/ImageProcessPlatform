/**
 * @file RuntimeViewDlg.h
 * @brief RuntimeViewDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_RUNTIMEVIEWDIALOG_H
#define VISION_RUNTIMEVIEWDIALOG_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QWidget;

namespace Vision {

struct Solution;

/**
 * @class RuntimePaneDlg
 * @brief 单个运行窗：一张底图，叠加可勾选多个出图形的工具（仅本工站）
 */
class VISION_UI_API RuntimePaneDlg : public QDialog {
public:
    /**
     * @brief 编辑一格的底图与叠加
     * @param sol 方案（不拥有）
     * @param st 工站（工具列表范围，不拥有）
     * @param index 窗口序号（从 0 起）
     * @param pane 当前配置
     */
    RuntimePaneDlg(Solution* sol, StationCfg* st, int index, const RuntimePane& pane,
                      QWidget* parent = 0);

    /** @brief 对话框里的窗格配置（含底图与多路叠加） */
    RuntimePane pane() const;

private:
    /** @brief 底图下拉：工站结果 + 采集、预处理 */
    void fillImageCombo();
    /** @brief 叠加列表：工站图形 + 定位、检测、结果处理 */
    void fillOverlayList();

    Solution* m_sol;          ///< 方案（不拥有）
    StationCfg* m_station; ///< 工站（不拥有）
    RuntimePane m_pane;       ///< 编辑中的窗格
    QComboBox* m_img;         ///< 底图
    QListWidget* m_ovList;    ///< 叠加工具（可多选）
    QCheckBox* m_showOv;      ///< 是否画叠加
};

/**
 * @class RuntimeViewDlg
 * @brief 配置多工站壳布局与单站 runtimeView：面板显隐、行列、每格底图
 */
class VISION_UI_API RuntimeViewDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 编辑方案级布局与指定工站的 runtimeView
     * @param sol 方案（写 runtimeStationLayout，不拥有）
     * @param st 初始编辑工站（不拥有）
     */
    RuntimeViewDlg(Solution* sol, StationCfg* st, QWidget* parent = 0);

private slots:
    /** @brief 切换编辑工站时加载其 runtimeView */
    void onStationChanged();
    /** @brief 按行列数隐藏多余窗格行 */
    void onLayoutChanged();
    /** @brief 打开当前行的窗口配置 */
    void onConfigPane();
    /** @brief 写回方案布局与当前工站并关闭 */
    void onSave();

private:
    /** @brief 把 UI 写回 m_station 的 runtimeView（切站前调用） */
    void flushCurrentStation();
    /** @brief 从 m_station 填充窗格 UI */
    void loadStationView();
    /** @brief 刷新一行的底图与叠加摘要 */
    void refreshSummary(int index);
    /** @brief 只显示当前布局用得到的行 */
    void applyRowVisible();

    Solution* m_sol;          ///< 方案（不拥有）
    StationCfg* m_station; ///< 当前编辑工站（不拥有）
    QComboBox* m_shellLayout; ///< 多工站：分页签 / 拼贴
    QComboBox* m_stationCombo;///< 编辑哪个工站
    QComboBox* m_rowCombo;    ///< 行数 1 至 3
    QComboBox* m_colCombo;    ///< 列数 1 至 3
    QCheckBox* m_stats;       ///< 产量统计
    QCheckBox* m_hist;        ///< 结果历史
    QCheckBox* m_side;        ///< 开始停止列
    RuntimePane m_panes[9];   ///< 各格配置
    QCheckBox* m_en[9];       ///< 各格使能
    QLabel* m_summary[9];     ///< 各格摘要
    QPushButton* m_config[9];    ///< 各格「配置」
    QWidget* m_lines[9];      ///< 窗格行容器
};

}

#endif
