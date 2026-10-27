#ifndef VISION_TOOLDEBUGHOST_H
#define VISION_TOOLDEBUGHOST_H
#include "VisionToolUiExport.h"

#include "RecipeDraft.h"
#include <HsCommData/HsImage.h>
#include <HsCommData/Graphic.h>
#include <QDialog>
#include <QSize>
#include <QVector>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QFrame;
class QGridLayout;
class QKeyEvent;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;

namespace HsCtrl {
class HsImageWindow;
}

namespace image_widget {
class ImageDrawPad;
}

namespace Vision {

class Job;
class Session;
class ToolEditorBase;
class ToolIoPanel;
struct Solution;
struct ToolCfg;
struct ToolOut;

/**
 * @class ToolDebugHost
 * @brief 详细调试：四宫格（图像/输入/ROI/参数）+ 底栏，draft 编辑
 */
class VISION_TOOLUI_API ToolDebugHost : public QDialog {
    Q_OBJECT
public:
    ToolDebugHost(ToolCfg* tool, Session* session, Solution* sol, Job* job,
                  QWidget* parent = 0);
    /** @brief 显示本工具运行图与叠加 */
    void showResult(const HsCommData::HsImage& fb, const QVector<HsCommData::Graphic>& overlay,
                    const QVector<HsCommData::Graphic>& drawings, bool replaceBase = true);
    /**
     * @brief 按工具输入与运行结果刷新图像窗
     * @param runOut 非空表示刚跑完；底图为输入图，叠加为本工具输出
     */
    void refreshView(const ToolOut* runOut = 0);
    /** @brief 按当前工具刷新输入摘要 */
    void refreshIo();
    /** @brief 仅按 draft 刷新图上 ROI，不改底图与叠加 */
    void reloadDrawings();
    /** @brief 把参数页写回 draft（运行前调用） */
    void applyParams();
    /** @brief 页内试跑用的工具 draft 视图 */
    const ToolCfg* draftTool() const;
    /** @brief 保存时是否标记 Solution::dirty（JobCfg 内调试为 false） */
    void setSolutionCommit(bool commit);
    /**
     * @brief 建议尺寸固定为打开尺寸
     * @return 1120x720 与屏幕可用区取小
     */
    QSize sizeHint() const;
    /** @brief 最小建议尺寸，不跟布局内容走 */
    QSize minimumSizeHint() const;
    /**
     * @brief 复检跳转：强制 draft/试跑用单文件图路径（不写 formal）
     * @param imagePath 图像全路径
     */
    void applyForcedImageFile(const std::string& imagePath);

signals:
    /** @brief 试跑完成（供父窗刷新） */
    void testRunFinished(const ToolOut& out);
    /** @brief ROI 已写回 draft */
    void roiChanged();
    /** @brief draft 已提交到 formal */
    void draftSaved();
    /** @brief 输入连线已改（刷新画布） */
    void linksChanged();

protected:
    /** @brief 关闭前确认是否丢弃未保存 draft */
    void closeEvent(QCloseEvent* event);
    /** @brief 首次显示后重算参数页高度并把窗口收到预设尺寸 */
    void showEvent(QShowEvent* event);
    /** @brief 拖动或缩放时把绘制小窗留在图像上方 */
    void moveEvent(QMoveEvent* event);
    void resizeEvent(QResizeEvent* event);
    /** @brief Ctrl+Z 撤销 ROI 修改 */
    void keyPressEvent(QKeyEvent* event);

private slots:
    /** @brief 页内试跑本工具 */
    void onRun();
    /** @brief 异步试跑完成（outPtr 为堆上 ToolOut*） */
    void onAsyncRunDone(void* outPtr);
    /** @brief 提交 draft 到 formal */
    void onSave();
    /** @brief 关闭并丢弃 draft */
    void onCancel();
    /** @brief 切换绘制工具 */
    void onDrawTool(int id);
    /** @brief 图上画完一笔 ROI */
    void onRoi(const HsCommData::Graphic& g);
    /** @brief 清空 ROI */
    void onClearRoi();
    /** @brief 删除最后一个 ROI */
    void onDelLastRoi();
    /** @brief 删除指定行 ROI */
    void onRemoveRoi(int index);
    /** @brief 表中选中 ROI 行变化 */
    void onRoiSelectionChanged(int index);
    /** @brief 「绘制模式」勾选变化 */
    void onDrawModeChanged();
    /** @brief 参数页改动（文件源路径等） */
    void onEditorEdited();
    /** @brief 输入下拉变更连线 */
    void onInputLinkChanged(int portData, const QString& fromId, const QString& fromPort);
    /** @brief 点「新增 ROI」进入绘制 */
    void onAddRoi();
    /** @brief 图上选中 ROI 变化，同步表格 */
    void onDrawingSelected(const HsCommData::Graphic& g);

private:
    /** @brief 打开尺寸：1120x720 与屏幕可用区取小 */
    static QSize preferredDebugSize(const QWidget* ref);
    /** @brief 把窗口尺寸与位置收进屏幕可用区 */
    void clampToScreen();
    /** @brief 绘制模式开/关时显隐工具条 */
    void updateDrawBarVisibility();
    /** @brief 按工具类型与已有 ROI 选默认形状工具 */
    int defaultDrawToolId() const;
    /** @brief 工具推荐的绘制工具 id */
    int preferredDrawToolId() const;
    /** @brief 把 draft ROI 刷到图上；refreshTable 为 false 时只切显示行，不重建表 */
    void applyDrawings(bool refreshTable = true);
    /** @brief 记住小窗提交时的画笔 */
    void rememberDrawStyle(int index, const HsCommData::Style& style);
    /** @brief 按选中行与绘制模式同步 Drawing 层 */
    void syncDrawingInteractionView();
    /** @brief 是否勾选连续绘制 */
    bool isDrawMode() const;
    /** @brief 更新 draft 脏标记与标题 */
    void markDraftDirty();
    /** @brief 刷新窗口标题（含未保存提示） */
    void updateWindowTitle();
    /** @brief 保存 draft 到 formal，不关窗 */
    bool trySave();
    /**
     * @brief 关窗前三选一：取消=0，不保存=1，已保存=2
     */
    int askCloseAction();
    /** @brief 提交 draft 到 formal */
    void commitDraft();
    /** @brief 修改 ROI 前压栈，供撤销 */
    void pushRoiUndo();
    /** @brief 撤销上一笔 ROI 修改 */
    void undoRoi();
    /** @brief ROI 种类与工具不匹配时提示 */
    void warnRoiKindMismatch();
    /** @brief 删除 ROI 前确认 */
    bool confirmRoiDelete(const QString& message) const;
    /** @brief 按工具能力重排四宫格（缺 ROI/参数时后续模块前移） */
    void rebuildQuadLayout();
    /** @brief 按工具能力显隐 ROI 卡片 */
    void updateRoiCardVisibility();
    /** @brief 按工具推荐 ROI 种类显隐形状按钮 */
    void updateDrawShapeTools();
    /** @brief 追加一行调试日志（带时间戳） */
    void appendDebugLog(const QString& line);
    /** @brief 把绘制小窗摆到图像上方 */
    void placeDrawPad();

    /**
     * @class RoiSnapshot
     * @brief 一笔 ROI 撤销记录
     */
    struct RoiSnapshot {
        std::vector<HsCommData::HsObject> rois; ///< ROI 列表快照
        std::vector<HsCommData::Style> drawStyles; ///< 小窗打开时该条的画笔
        std::vector<char> drawStyleOn; ///< 1 表示该条记住了画笔
    };

    HsCtrl::HsImageWindow* m_view; ///< 调试图
    image_widget::ImageDrawPad* m_drawPad; ///< 图像旁边的绘制条，自身是工具窗
    std::vector<HsCommData::Style> m_drawStyles; ///< 小窗打开期间各条 ROI 的画笔
    std::vector<char> m_drawStyleOn; ///< 1 表示该下标记住了画笔
    ToolEditorBase* m_editor;       ///< 算法参数页
    ToolIoPanel* m_io;              ///< 连接与 ROI 数据
    QWidget* m_quadPanel;           ///< 四宫格容器
    QGridLayout* m_quadGrid;        ///< 四宫格布局
    QFrame* m_imageWrap;            ///< 图像区
    QFrame* m_linkCard;             ///< 输入卡片
    QFrame* m_roiCard;              ///< ROI 卡片
    QFrame* m_paramCard;            ///< 参数卡片
    QWidget* m_quadEmpty[2];        ///< 空缺宫格占位
    QPlainTextEdit* m_log;          ///< 底栏试跑与操作日志
    ToolCfg* m_formalTool;
    ObjectId m_boundId; ///< 打开时复制的工具 id，关窗不再读可能已失效的配置          ///< 方案内正式工具，不拥有
    ToolCfg m_draftView;            ///< 本会话 draft
    ToolRecipe m_formalSnapshot;    ///< Open 时 formal 配方快照
    ToolCfg* m_tool;                ///< 指向 m_draftView，供编辑逻辑复用
    Session* m_session;             ///< 会话，不拥有
    Solution* m_sol;                ///< 方案，不拥有
    Job* m_job;                     ///< 所属流程运行实例，不拥有
    bool m_commitToSolution;        ///< Save 是否置 Solution::dirty
    QButtonGroup* m_drawTools;      ///< 浏览与形状工具互斥组
    QWidget* m_drawBar;             ///< 形状按钮容器
    QFrame* m_drawPanel;            ///< 形状工具区（绘制模式开时显示）
    QWidget* m_drawModeWrap;        ///< 绘制模式行（无 ROI 能力时隐藏）
    QCheckBox* m_chkDrawMode;       ///< 连续绘制：画完不切回浏览
    bool m_pendingNewRoi;           ///< 点新增/重画后待画新 ROI
    bool m_syncingRoiSelection;     
    QVector<RoiSnapshot> m_roiUndo; ///< ROI 撤销栈
    std::string m_forcedImagePath;  ///< 复检强制图路径；空=不强制
    bool m_runBusy;                 ///< 试跑已投递未完成
};

}

#endif
