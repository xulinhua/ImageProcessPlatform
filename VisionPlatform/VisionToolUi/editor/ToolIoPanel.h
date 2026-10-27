/**
 * @file ToolIoPanel.h
 * @brief ToolIoPanel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLIOPANEL_H
#define VISION_TOOLIOPANEL_H
#include "VisionToolUiExport.h"

#include "Solution.h"
#include <HsCommData/HsImage.h>
#include <QWidget>
#include <vector>

class QButtonGroup;
class QComboBox;
class QEvent;
class QLabel;
class QPushButton;
class QTableWidget;

namespace HsCtrl {
class HsImageWindow;
}

namespace Vision {

class Job;
struct Solution;
struct ToolCfg;

/** @brief ToolIoPanel 子页：连接与 ROI 分区 */
enum class ToolIoPage {
    Link = 0, ///< 输入连接
    Roi = 1   ///< 检测区域
};

/**
 * @class ToolIoPanel
 * @brief 工具输入：连接页与 ROI 页，供调试窗分栏展示
 */
class VISION_TOOLUI_API ToolIoPanel : public QWidget {
    Q_OBJECT
public:
    /** @brief 连接页与 ROI 页 */
    explicit ToolIoPanel(QWidget* parent = 0);

    /**
     * @brief 按当前工具与流程缓存刷新控件
     * @param canEdit 有方案编辑权限时才允许改连线与 ROI
     */
    void refresh(Solution* sol, ToolCfg* tool, Job* job, bool canEdit);

    /** @brief ROI 表当前选中行；无选中时 -1 */
    int selectedRoiRow() const;

    /** @brief 取消表格选中（不触发 roiSelectionChanged） */
    void clearRoiSelection();

    /**
     * @brief 选中指定行（不触发 roiSelectionChanged）
     * @param row 从 0 起；越界则清空选中
     */
    void selectRoiRow(int row);

    /** @brief 指定子页根控件（不拥有，生命周期同本 panel） */
    QWidget* pageWidget(ToolIoPage page) const;

signals:
    /** @brief 清空本工具 ROI */
    void clearRoiRequested();
    /** @brief 新增 ROI（进入绘制） */
    void addRoiRequested();
    /** @brief 删除最后一个 ROI */
    void removeLastRoiRequested();
    /**
     * @brief 删除指定行 ROI
     * @param index 表中从 0 起的行号
     */
    void removeRoiRequested(int index);
    /**
     * @brief 表中选中行变化
     * @param index 从 0 起的行号；无选中时为 -1
     */
    void roiSelectionChanged(int index);
    /**
     * @brief 输入端口连线变更
     * @param portData PortData 整型值
     * @param fromId 上游工具 id；空表示断开
     * @param fromPort 上游端口键，如 out.image
     */
    void inputLinkChanged(int portData, const QString& fromId, const QString& fromPort);

private slots:
    /** @brief 删除选中行；无选中则删末个 */
    void onDeleteClicked();
    /** @brief 表内行删除按钮 */
    void onDeleteRowClicked();
    /** @brief 表选中行变化时通知外部刷新图上 ROI */
    void onRoiSelectionChanged();
    /** @brief 输入下拉选中变化 */
    void onLinkComboChanged(int index);

protected:
    /** @brief 表格 viewport：再点已选行则取消选中 */
    bool eventFilter(QObject* watched, QEvent* event);

private:
    /**
     * @brief 按 ROI 种类填表
     * @param rs 当前工具有效 ROI
     */
    void fillRoiTable(const std::vector<HsCommData::HsObject>& rs);
    /** @brief 刷新单个输入端口下拉与状态行 */
    void refreshLinkRow(QComboBox* combo, QLabel* hint, QLabel* camText,
                        const JobCfg* jobConfig, const ToolCfg* tool, Job* job,
                        PortData data, bool canEdit, bool showRow);
    /** @brief 填充输入端口候选来源 */
    void fillLinkCombo(QComboBox* combo, const JobCfg& job, const ObjectId& toId, PortData data);
    /** @brief 选中当前已连线的项 */
    void selectCurrentLink(QComboBox* combo, const JobCfg& job, const ObjectId& toId, const PortSlot& toPort);

    QWidget* m_linkPage;     ///< 连接页根
    QWidget* m_roiPage;      ///< ROI 页根
    QLabel* m_imgRowLabel;   ///< 图像源行标题
    QLabel* m_imgCamText;    ///< 采集工具固定说明
    QComboBox* m_imgCombo;   ///< 图像输入来源
    QLabel* m_imgHint;       ///< 图像尺寸或运行状态
    QLabel* m_poseRowLabel;  ///< 位姿/逻辑行标题
    QComboBox* m_poseCombo;  ///< 位姿或逻辑输入
    QLabel* m_poseHint;      ///< 位姿/逻辑缓存摘要
    QLabel* m_regionRowLabel; ///< 区域源行标题
    QComboBox* m_regionCombo; ///< 区域输入来源
    QLabel* m_regionHint;    ///< 区域连线状态
    QTableWidget* m_roiTable;  ///< ROI 表
    QPushButton* m_btnAdd;     ///< 新增 ROI
    QPushButton* m_btnRedraw;  ///< 单 ROI 工具重画
    QPushButton* m_btnClear;   ///< 清空 ROI
    QPushButton* m_btnDel;     ///< 删除选中 ROI
    bool m_fillingRoiTable;    ///< 正在填表，避免 selectRow 递归触发
    bool m_fillingLinks;       ///< 正在填下拉，避免 currentIndexChanged 递归
    int m_pressSelectedRow;    ///< 按下时已选中的行，用于 toggle 取消
};

/**
 * @brief 把 ROI 明细叠到图像窗（图像坐标）
 * @param onlyIndex 非负时只显示该行明细；-1 显示全部
 * @param showLabels 为 false 时只清标注，不绘制文字（浏览/非绘制模式）
 */
VISION_TOOLUI_API void overlayRoiDetails(HsCtrl::HsImageWindow* view, const ToolCfg* tool, int onlyIndex = -1,
                       bool showLabels = false);

/**
 * @brief 调试窗底图：采集工具用本工具缓存，其余用连线输入图
 * @return 无合适输入时返回空帧
 */
VISION_TOOLUI_API HsCommData::HsImage debugBaseFrame(ToolCfg* tool, JobCfg* jobConfig, Job* job);

/**
 * @brief 按选中 ROI 与是否可编辑同步 Drawing 层
 * @param editable 绘制模式开：可拖动手柄；关：只读显示
 * @param selectedIndex 表中选中行；-1 表示不限定单条（主工作区显示全部）
 * @param hasDrawings 是否有 ROI 需要显示
 */
VISION_TOOLUI_API void syncDrawingInteraction(HsCtrl::HsImageWindow* view, bool editable,
                            int selectedIndex, bool hasDrawings);

}

#endif
