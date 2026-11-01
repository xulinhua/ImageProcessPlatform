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
class QVBoxLayout;

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
 * @brief 工具输入：连接页与 ROI 页（绘制/引用，对齐 BV），供调试窗分栏展示
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

    /** @brief ROI 表当前选中行（roiItems 下标）；无选中时 -1 */
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
    /** @brief 新增 ROI（进入绘制或追加空项） */
    void addRoiRequested();
    /** @brief 删除最后一个 ROI */
    void removeLastRoiRequested();
    /**
     * @brief 删除指定行 ROI（roiItems 下标）
     */
    void removeRoiRequested(int index);
    /**
     * @brief 表中选中行变化
     */
    void roiSelectionChanged(int index);
    /**
     * @brief 对指定行发起绘制（roiItems 下标）
     */
    void drawRoiRequested(int index);
    /** @brief ROI 项内容已就地改写（种类/引用/使能），需刷新图与脏标记 */
    void roiItemsEdited();
    /**
     * @brief 输入端口连线变更
     */
    void inputLinkChanged(int portData, const QString& fromId, const QString& fromPort);
    /** @brief 全局变量绑定表已改写，需标脏 */
    void paramBindsEdited();

private slots:
    /** @brief 删除选中行；无选中则删末个 */
    void onDeleteClicked();
    /** @brief 行内删除 */
    void onDeleteRowClicked();
    /** @brief 输入下拉选中变化 */
    void onLinkComboChanged(int index);
    /** @brief 选中某一 ROI 行 */
    void onSelectRoiRow();
    /** @brief 种类下拉：绘制 / 引用 */
    void onRoiKindChanged(int index);
    /** @brief 引用源下拉 */
    void onRoiRefChanged(int index);
    /** @brief 引用序号 */
    void onRoiRefIndexChanged(int value);
    /** @brief 使能勾选 */
    void onRoiEnableToggled(bool on);
    /** @brief 行内绘制按钮 */
    void onDrawRowClicked();
    /** @brief 追加一条全局变量绑定 */
    void onAddParamBind();
    /** @brief 删除选中绑定行 */
    void onDelParamBind();
    /** @brief 绑定表单元格改写回 ToolParam */
    void onParamBindCellChanged(int row, int column);

protected:
    /** @brief 预留；列表点击由行控件处理 */
    bool eventFilter(QObject* watched, QEvent* event);

private:
    /** @brief 按 roiItems 重建列表（绘制/引用行） */
    void fillRoiList();
    /** @brief 追加一行 ROI 控件（fillRoiList 用） */
    void appendRoiRow(int index, const ToolRoiItem& item, bool allowRef, bool allowDraw,
                      const JobCfg* jobConfig, const std::vector<HsCommData::HsObject>& preview);
    /** @brief 刷新单个输入端口下拉与状态行 */
    void refreshLinkRow(QComboBox* combo, QLabel* hint, QLabel* camText,
                        const JobCfg* jobConfig, const ToolCfg* tool, Job* job,
                        PortData data, bool canEdit, bool showRow);
    /** @brief 填充输入端口候选来源 */
    void fillLinkCombo(QComboBox* combo, const JobCfg& job, const ObjectId& toId, PortData data);
    /** @brief 选中当前已连线的项 */
    void selectCurrentLink(QComboBox* combo, const JobCfg& job, const ObjectId& toId, const PortSlot& toPort);
    /** @brief 槽位是否允许引用 */
    static bool slotAllowsRef(const ToolCaps& caps);
    /** @brief 槽位是否允许绘制 */
    static bool slotAllowsDraw(const ToolCaps& caps);
    /** @brief 默认分区键 */
    static std::string defaultRoiSlot(const ToolCaps& caps);
    /** @brief 按 tool.param.paramBinds 重建绑定表 */
    void fillParamBinds();
    /** @brief 把绑定表写回 tool.param.paramBinds */
    void commitParamBindsFromTable();

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
    QLabel* m_bindLabel;       ///< 全局变量绑定标题
    QTableWidget* m_bindTable; ///< 参数键 / 路径 / 方向
    QPushButton* m_btnBindAdd; ///< 添加绑定
    QPushButton* m_btnBindDel; ///< 删除绑定
    QWidget* m_roiListHost;    ///< ROI 行列表宿主
    QVBoxLayout* m_roiListLay; ///< ROI 行布局
    QPushButton* m_btnAdd;     ///< 新增 ROI
    QPushButton* m_btnRedraw;  ///< 单 ROI 工具重画
    QPushButton* m_btnClear;   ///< 清空 ROI
    QPushButton* m_btnDel;     ///< 删除选中 ROI
    bool m_fillingRoiList;     ///< 正在填表，避免递归
    bool m_fillingLinks;       ///< 正在填下拉，避免 currentIndexChanged 递归
    bool m_fillingBinds;       ///< 正在填绑定表
    int m_selectedRoiRow;      ///< 当前选中 roiItems 下标
    ToolCfg* m_tool;           ///< 最近一次 refresh 的工具
    Solution* m_sol;           ///< 最近一次 refresh 的方案
    Job* m_job;                ///< 最近一次 refresh 的流程
    bool m_canEdit;            ///< 是否可编辑
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
